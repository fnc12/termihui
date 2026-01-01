#include "TermihuiServerController.h"
#include "JsonHelper.h"
#include "hv/json.hpp"
#include <fmt/core.h>
#include <thread>

using json = nlohmann::json;

// Static member initialization
std::atomic<bool> TermihuiServerController::shouldExit{false};

TermihuiServerController::TermihuiServerController(std::unique_ptr<WebSocketServer> webSocketServer)
    : webSocketServer(std::move(webSocketServer))
    , lastStatsTime(std::chrono::steady_clock::now())
{
}

TermihuiServerController::~TermihuiServerController() {
    this->stop();
}

void TermihuiServerController::signalHandler(int signal) {
    fmt::print("\nReceived signal {}, shutting down...\n", signal);
    shouldExit.store(true);
}

bool TermihuiServerController::start() {
    // Initialize file system manager and create storage directory
    this->fileSystemManager.initialize();
    fmt::print("📁 Data storage path: {}\n", this->fileSystemManager.getWritablePath().string());
    
    // Initialize server storage and record start
    auto serverDbPath = this->fileSystemManager.getWritablePath() / "server_state.sqlite";
    this->serverStorage = std::make_unique<ServerStorage>(serverDbPath);
    
    if (this->serverStorage->wasLastRunCrashed()) {
        fmt::print("⚠️  Previous server run was not properly shut down\n");
    }
    
    this->currentRunId = this->serverStorage->recordStart();
    fmt::print("🚀 Server run ID: {}\n", this->currentRunId);
    
    // Start WebSocket server
    if (!this->webSocketServer->start()) {
        fmt::print(stderr, "Failed to start WebSocket server on {}:{}\n", 
                   this->webSocketServer->getBindAddress(), this->webSocketServer->getPort());
        return false;
    }
    
    // Sessions are created on demand when client requests them
    fmt::print("Server started, waiting for clients\n");
    return true;
}

void TermihuiServerController::stop() {
    // Terminate all sessions
    for (auto& [sessionId, controller] : this->sessions) {
        controller->terminate();
    }
    this->sessions.clear();
    
    this->webSocketServer->stop();
    
    // Record graceful stop
    if (this->serverStorage && this->currentRunId > 0) {
        this->serverStorage->recordStop(this->currentRunId);
    }
    
    fmt::print("Server stopped\n");
}

void TermihuiServerController::update() {
    // Get events from WebSocket server
    auto updateResult = this->webSocketServer->update();
    
    // Handle connection/disconnection events
    for (const auto& connectionEvent : updateResult.connectionEvents) {
        if (connectionEvent.connected) {
            this->handleNewConnection(connectionEvent.clientId);
        } else {
            this->handleDisconnection(connectionEvent.clientId);
        }
    }
    
    // Handle incoming messages
    for (const auto& incomingMessage : updateResult.incomingMessages) {
        this->handleMessage(incomingMessage);
    }
    
    // Process terminal output for all sessions
    for (auto& [sessionId, controller] : this->sessions) {
        this->processTerminalOutput(*controller);
    
    // Check session status and send completion notification
        if (controller->didJustFinishRunning()) {
            fmt::print("Session {} command completed\n", sessionId);
            json status;
            status["type"] = "status";
            status["session_id"] = sessionId;
            status["running"] = false;
            this->webSocketServer->broadcastMessage(status.dump());
        }
    }
    
    // Print statistics every 30 seconds
    this->printStats();
    
    // Small pause to avoid CPU overload
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void TermihuiServerController::handleNewConnection(int clientId) {
    fmt::print("Client connected: {}\n", clientId);
    
    // Send welcome message
    json welcomeMsg;
    welcomeMsg["type"] = "connected";
    welcomeMsg["server_version"] = "1.0.0";
    // Add home directory for path shortening on client
    if (const char* home = getenv("HOME")) {
        welcomeMsg["home"] = home;
    }
    this->webSocketServer->sendMessage(clientId, welcomeMsg.dump());
    
    // Client will request session list and history separately
}

void TermihuiServerController::handleDisconnection(int clientId) {
    fmt::print("Client disconnected: {}\n", clientId);
}

void TermihuiServerController::handleMessage(const WebSocketServer::IncomingMessage& message) {
    fmt::print("Processing message from {}: {}\n", message.clientId, message.text);
    
    try {
        json messageJson = json::parse(message.text);
        std::string type = messageJson.at("type").get<std::string>();
        
        if (type == "execute") {
            this->handleExecuteMessage(message.clientId, messageJson.at("session_id").get<uint64_t>(), messageJson.at("command").get<std::string>());
        } else if (type == "input") {
            this->handleInputMessage(message.clientId, messageJson.at("session_id").get<uint64_t>(), messageJson.at("text").get<std::string>());
        } else if (type == "completion") {
            this->handleCompletionMessage(
                message.clientId,
                messageJson.at("session_id").get<uint64_t>(),
                messageJson.at("text").get<std::string>(),
                messageJson.at("cursor_position").get<int>()
            );
        } else if (type == "resize") {
            this->handleResizeMessage(
                message.clientId,
                messageJson.at("session_id").get<uint64_t>(),
                messageJson.at("cols").get<int>(),
                messageJson.at("rows").get<int>()
            );
        } else if (type == "list_sessions") {
            this->handleListSessionsMessage(message.clientId);
        } else if (type == "create_session") {
            this->handleCreateSessionMessage(message.clientId);
        } else if (type == "close_session") {
            this->handleCloseSessionMessage(message.clientId, messageJson.at("session_id").get<uint64_t>());
        } else if (type == "get_history") {
            this->handleGetHistoryMessage(message.clientId, messageJson.at("session_id").get<uint64_t>());
        } else {
            std::string error = JsonHelper::createResponse("error", "Unknown message type: " + type);
            this->webSocketServer->sendMessage(message.clientId, error);
        }
    } catch (const json::exception& e) {
        fmt::print(stderr, "JSON error: {}\n", e.what());
        std::string error = JsonHelper::createResponse("error", std::string("Invalid message: ") + e.what());
        this->webSocketServer->sendMessage(message.clientId, error);
    }
}

TerminalSessionController* TermihuiServerController::findSession(uint64_t sessionId) {
    auto it = this->sessions.find(sessionId);
    if (it != this->sessions.end()) {
        return it->second.get();
    }
    
    // Lazy initialization: check if session exists in DB
    if (!this->serverStorage->isActiveTerminalSession(sessionId)) {
        return nullptr;
    }
    
    // Create controller on-demand
    auto sessionDbPath = this->fileSystemManager.getWritablePath() / fmt::format("session_{}.sqlite", sessionId);
    auto controller = std::make_unique<TerminalSessionController>(
        sessionDbPath, sessionId, this->currentRunId);
    
    if (!controller->createSession()) {
        fmt::print(stderr, "Failed to lazily create session {}\n", sessionId);
        return nullptr;
    }
    
    fmt::print("Lazily created session controller for session {}\n", sessionId);
    auto* ptr = controller.get();
    this->sessions[sessionId] = std::move(controller);
    return ptr;
}

void TermihuiServerController::handleExecuteMessage(int clientId, uint64_t sessionId, const std::string& command) {
    auto* terminalSessionController = this->findSession(sessionId);
    if (!terminalSessionController) {
        std::string error = JsonHelper::createResponse("error", fmt::format("Session {} not found", sessionId));
        this->webSocketServer->sendMessage(clientId, error);
        return;
    }
    
    terminalSessionController->setPendingCommand(command);
    
    using ExecuteCommandResult = TerminalSessionController::ExecuteCommandResult;
    const ExecuteCommandResult executeCommandResult = terminalSessionController->executeCommand(command);
    
    if (executeCommandResult.isOk()) {
        fmt::print("Session {}: Executed command: {}\n", sessionId, command);
    } else {
        std::string errorText = fmt::format("Failed to execute command *{}*: {}", command, executeCommandResult.errorText());
        std::string error = JsonHelper::createResponse("error", std::move(errorText));
        this->webSocketServer->sendMessage(clientId, error);
    }
}

void TermihuiServerController::handleInputMessage(int clientId, uint64_t sessionId, const std::string& text) {
    auto* terminalSessionController = this->findSession(sessionId);
    if (!terminalSessionController) {
        std::string error = JsonHelper::createResponse("error", fmt::format("Session {} not found", sessionId));
        this->webSocketServer->sendMessage(clientId, error);
        return;
    }
    
    ssize_t bytes = terminalSessionController->sendInput(text);
                if (bytes >= 0) {
        std::string response = JsonHelper::createResponse("input_sent", "", int(bytes));
        this->webSocketServer->sendMessage(clientId, response);
                } else {
                    std::string error = JsonHelper::createResponse("error", "Failed to send input");
        this->webSocketServer->sendMessage(clientId, error);
    }
}
            
void TermihuiServerController::handleCompletionMessage(int clientId, uint64_t sessionId, const std::string& text, int cursorPosition) {
    fmt::print("Completion request for session {}: '{}' (position: {})\n", sessionId, text, cursorPosition);
    
    auto* terminalSessionController = this->findSession(sessionId);
    std::string currentDir = ".";
    if (terminalSessionController) {
        currentDir = terminalSessionController->getLastKnownCwd();
        if (currentDir.empty()) {
            currentDir = terminalSessionController->getCurrentWorkingDirectory();
        }
    }
    if (currentDir.empty()) {
        currentDir = ".";
    }
    
    auto completions = this->completionManager.getCompletions(text, cursorPosition, currentDir);
    
            json response;
            response["type"] = "completion_result";
            response["completions"] = completions;
            response["original_text"] = text;
            response["cursor_position"] = cursorPosition;
            
    this->webSocketServer->sendMessage(clientId, response.dump());
}

void TermihuiServerController::handleResizeMessage(int clientId, uint64_t sessionId, int cols, int rows) {
    if (cols <= 0 || rows <= 0) {
        std::string error = JsonHelper::createResponse("error", "Invalid terminal size");
        this->webSocketServer->sendMessage(clientId, error);
        return;
    }
    
    auto* terminalSessionController = this->findSession(sessionId);
    if (!terminalSessionController) {
        std::string error = JsonHelper::createResponse("error", fmt::format("Session {} not found", sessionId));
        this->webSocketServer->sendMessage(clientId, error);
        return;
    }
    
    if (terminalSessionController->setWindowSize(static_cast<unsigned short>(cols), static_cast<unsigned short>(rows))) {
                    json response;
                    response["type"] = "resize_ack";
                    response["cols"] = cols;
                    response["rows"] = rows;
        this->webSocketServer->sendMessage(clientId, response.dump());
                } else {
                    std::string error = JsonHelper::createResponse("error", "Failed to set terminal size");
        this->webSocketServer->sendMessage(clientId, error);
    }
}

void TermihuiServerController::handleListSessionsMessage(int clientId) {
    auto sessions = this->serverStorage->getActiveTerminalSessions();
    
    json response;
    response["type"] = "sessions_list";
    json sessionsArray = json::array();
    for (const auto& s : sessions) {
        sessionsArray.push_back({
            {"id", s.id},
            {"created_at", s.createdAt}
        });
    }
    response["sessions"] = std::move(sessionsArray);
    
    this->webSocketServer->sendMessage(clientId, response.dump());
    fmt::print("Sent sessions list ({} sessions) to client {}\n", sessions.size(), clientId);
}

void TermihuiServerController::handleCreateSessionMessage(int clientId) {
    // Create session record in DB
    uint64_t sessionId = this->serverStorage->createTerminalSession(this->currentRunId);
    
    // Create terminal session controller
    auto sessionDbPath = this->fileSystemManager.getWritablePath() / fmt::format("session_{}.sqlite", sessionId);
    auto controller = std::make_unique<TerminalSessionController>(
        sessionDbPath, sessionId, this->currentRunId);
    
    if (!controller->createSession()) {
        std::string error = JsonHelper::createResponse("error", "Failed to create terminal session");
        this->webSocketServer->sendMessage(clientId, error);
        // TODO: cleanup DB record
        return;
    }
    
    this->sessions[sessionId] = std::move(controller);
    
    json response;
    response["type"] = "session_created";
    response["session_id"] = sessionId;
    
    this->webSocketServer->sendMessage(clientId, response.dump());
    fmt::print("Created session {} for client {}\n", sessionId, clientId);
}

void TermihuiServerController::handleCloseSessionMessage(int clientId, uint64_t sessionId) {
    auto it = this->sessions.find(sessionId);
    if (it == this->sessions.end()) {
        std::string error = JsonHelper::createResponse("error", fmt::format("Session {} not found", sessionId));
        this->webSocketServer->sendMessage(clientId, error);
        return;
    }
    
    // Terminate and remove session
    it->second->terminate();
    this->sessions.erase(it);
    
    // Mark as deleted in DB
    this->serverStorage->markTerminalSessionAsDeleted(sessionId);
    
    json response;
    response["type"] = "session_closed";
    response["session_id"] = sessionId;
    
    this->webSocketServer->sendMessage(clientId, response.dump());
    fmt::print("Closed session {} for client {}\n", sessionId, clientId);
}

void TermihuiServerController::handleGetHistoryMessage(int clientId, uint64_t sessionId) {
    auto* terminalSessionController = this->findSession(sessionId);
    if (!terminalSessionController) {
        std::string error = JsonHelper::createResponse("error", fmt::format("Session {} not found", sessionId));
        this->webSocketServer->sendMessage(clientId, error);
        return;
    }
    
    auto commandHistory = terminalSessionController->getCommandHistory();
    
    json historyMsg;
    historyMsg["type"] = "history";
    historyMsg["session_id"] = sessionId;
    json commands = json::array();
    for (const auto& record : commandHistory) {
        json command = json::object();
        command["id"] = record.id;
        command["command"] = record.command;
        command["output"] = record.output;
        command["exit_code"] = record.exitCode;
        command["cwd_start"] = record.cwdStart;
        command["cwd_end"] = record.cwdEnd;
        command["is_finished"] = record.isFinished;
        commands.push_back(std::move(command));
    }
    historyMsg["commands"] = std::move(commands);
    
    this->webSocketServer->sendMessage(clientId, historyMsg.dump());
    fmt::print("Sent history for session {} ({} commands) to client {}\n", sessionId, commandHistory.size(), clientId);
}


// Helper: escape string for logging (show control chars)
static std::string escapeForLog(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '\x1b') {
            result += "\\e";
        } else if (c == '\x07') {
            result += "\\a";
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else if (c < 32) {
            result += fmt::format("\\x{:02x}", static_cast<unsigned char>(c));
        } else {
            result += c;
        }
    }
    return result;
}

void TermihuiServerController::processTerminalOutput(TerminalSessionController& session) {
    if (!session.hasData()) {
        return;
    }
    
    std::string output = session.readOutput();
    if (output.empty()) {
        return;
    }
    
    fmt::print("[OSC-PARSE] Raw output ({} bytes): {}\n", output.size(), escapeForLog(output));
    
    // Helper: find next OSC sequence (any type)
    auto findNextOSC = [&output](size_t from) -> size_t {
        return output.find("\x1b]", from);
    };
    
    // Helper: extract path from "user@host:path" format
    auto extractPathFromTitle = [](const std::string& title) -> std::string {
        // Format: "user@host:path" or just "path"
        size_t colonPos = title.rfind(':');
        if (colonPos != std::string::npos && colonPos < title.length() - 1) {
            // Check if there's @ before : (indicates user@host:path format)
            size_t atPos = title.find('@');
            if (atPos != std::string::npos && atPos < colonPos) {
                return title.substr(colonPos + 1);
            }
        }
        return "";
    };
    
    // Streaming parser: preserve order "text → event → text"
    size_t i = 0;
    int iteration = 0;
    while (true) {
        iteration++;
        size_t oscPos = findNextOSC(i);
        fmt::print("[OSC-PARSE] Iteration {}: i={}, oscPos={}\n", iteration, i, 
                   oscPos == std::string::npos ? -1 : static_cast<int>(oscPos));
        
        if (oscPos == std::string::npos) {
            // Remainder as regular output
            if (i < output.size()) {
                std::string chunk = output.substr(i);
                if (!chunk.empty()) {
                    fmt::print("[OSC-PARSE] Final text chunk: {}\n", escapeForLog(chunk));
                    session.appendOutputToCurrentCommand(chunk);
                    this->webSocketServer->broadcastMessage(JsonHelper::createResponse("output", chunk));
                }
            }
            break;
        }

        // Send text before marker
        if (oscPos > i) {
            std::string chunk = output.substr(i, oscPos - i);
            if (!chunk.empty()) {
                fmt::print("[OSC-PARSE] Text before OSC: {}\n", escapeForLog(chunk));
                session.appendOutputToCurrentCommand(chunk);
                this->webSocketServer->broadcastMessage(JsonHelper::createResponse("output", chunk));
            }
        }

        // Find OSC end (BEL or ST)
        size_t oscEnd = output.find('\x07', oscPos);
        // Also check for ST (ESC \)
        size_t stPos = output.find("\x1b\\", oscPos);
        if (stPos != std::string::npos && (oscEnd == std::string::npos || stPos < oscEnd)) {
            oscEnd = stPos + 1; // Point to after ST
        }
        
        fmt::print("[OSC-PARSE] OSC boundaries: start={}, end={}\n", oscPos, 
                   oscEnd == std::string::npos ? -1 : static_cast<int>(oscEnd));
        
        if (oscEnd == std::string::npos) {
            // Incomplete marker — treat rest as regular text
            std::string chunk = output.substr(oscPos);
            if (!chunk.empty()) {
                fmt::print("[OSC-PARSE] Incomplete OSC, treating as text: {}\n", escapeForLog(chunk));
                session.appendOutputToCurrentCommand(chunk);
                this->webSocketServer->broadcastMessage(JsonHelper::createResponse("output", chunk));
            }
            break;
        }

        std::string osc = output.substr(oscPos, oscEnd - oscPos + 1);
        fmt::print("[OSC-PARSE] Found OSC sequence: {}\n", escapeForLog(osc));
        
        // Helper lambda to extract parameter value from OSC string
        auto extractParam = [&osc](const std::string& key) -> std::string {
            auto pos = osc.find(key + "=");
            if (pos == std::string::npos) return "";
            size_t start = pos + key.length() + 1;
            size_t end = osc.find_first_of(";\x07", start);
            if (end == std::string::npos) end = osc.length();
            return osc.substr(start, end - start);
        };
        
        // Process OSC based on type
        if (osc.rfind("\x1b]133;A", 0) == 0) {
            // OSC 133;A - Command start (our marker)
            std::string cwd = extractParam("cwd");
            fmt::print("[OSC-PARSE] >>> OSC 133;A (command_start) cwd={}\n", cwd);
            if (!cwd.empty()) {
                session.setLastKnownCwd(cwd);
            }
            
            session.startCommandInHistory(cwd);
            
            if (session.hasActiveCommand()) {
                json ev;
                ev["type"] = "command_start";
                if (!cwd.empty()) {
                    ev["cwd"] = cwd;
                }
                this->webSocketServer->broadcastMessage(ev.dump());
            }
        } else if (osc.rfind("\x1b]133;B", 0) == 0) {
            // OSC 133;B - Command end (our marker)
            int exitCode = 0;
            auto k = osc.find("exit=");
            if (k != std::string::npos) exitCode = std::atoi(osc.c_str() + k + 5);
            std::string cwd = extractParam("cwd");
            fmt::print("[OSC-PARSE] >>> OSC 133;B (command_end) exit={}, cwd={}\n", exitCode, cwd);
            if (!cwd.empty()) {
                session.setLastKnownCwd(cwd);
            }
            
            if (session.hasActiveCommand()) {
                session.finishCurrentCommand(exitCode, cwd);
                
                json ev;
                ev["type"] = "command_end";
                ev["exit_code"] = exitCode;
                if (!cwd.empty()) {
                    ev["cwd"] = cwd;
                }
                this->webSocketServer->broadcastMessage(ev.dump());
            }
        } else if (osc.rfind("\x1b]133;C", 0) == 0) {
            // OSC 133;C - Prompt start
            fmt::print("[OSC-PARSE] >>> OSC 133;C (prompt_start)\n");
            json ev;
            ev["type"] = "prompt_start";
            this->webSocketServer->broadcastMessage(ev.dump());
        } else if (osc.rfind("\x1b]133;D", 0) == 0) {
            // OSC 133;D - Prompt end
            fmt::print("[OSC-PARSE] >>> OSC 133;D (prompt_end)\n");
            json ev;
            ev["type"] = "prompt_end";
            this->webSocketServer->broadcastMessage(ev.dump());
        } else if (osc.rfind("\x1b]2;", 0) == 0) {
            // OSC 2 - Window title (often contains user@host:path)
            // Extract title content between "ESC]2;" and BEL/ST
            size_t titleStart = 4; // After "ESC]2;"
            size_t titleEnd = osc.find_first_of("\x07\x1b", titleStart);
            if (titleEnd != std::string::npos) {
                std::string title = osc.substr(titleStart, titleEnd - titleStart);
                std::string path = extractPathFromTitle(title);
                fmt::print("[OSC-PARSE] >>> OSC 2 (window_title) title={}, extracted_path={}\n", title, path);
                if (!path.empty()) {
                    session.setLastKnownCwd(path);
                    
                    // Send cwd_update event to client
                    json ev;
                    ev["type"] = "cwd_update";
                    ev["cwd"] = path;
                    this->webSocketServer->broadcastMessage(ev.dump());
                }
            }
        } else if (osc.rfind("\x1b]7;", 0) == 0) {
            // OSC 7 - Current working directory (file://host/path format)
            size_t pathStart = osc.find("file://");
            if (pathStart != std::string::npos) {
                pathStart += 7; // Skip "file://"
                // Skip hostname (find next /)
                size_t slashPos = osc.find('/', pathStart);
                if (slashPos != std::string::npos) {
                    size_t pathEnd = osc.find_first_of("\x07\x1b", slashPos);
                    if (pathEnd != std::string::npos) {
                        std::string path = osc.substr(slashPos, pathEnd - slashPos);
                        fmt::print("[OSC-PARSE] >>> OSC 7 (cwd) path={}\n", path);
                        session.setLastKnownCwd(path);
                        
                        // Send cwd_update event to client
                        json ev;
                        ev["type"] = "cwd_update";
                        ev["cwd"] = path;
                        this->webSocketServer->broadcastMessage(ev.dump());
                    }
                }
            } else {
                fmt::print("[OSC-PARSE] >>> OSC 7 (cwd) - no file:// found\n");
            }
        } else {
            // Other OSC types (1, etc.) 
            fmt::print("[OSC-PARSE] >>> Unknown OSC type, ignoring\n");
        }

        // Continue after marker
        i = oscEnd + 1;
    }
}

void TermihuiServerController::printStats() {
    auto now = std::chrono::steady_clock::now();
    if (now - this->lastStatsTime > std::chrono::seconds(30)) {
        size_t connectedClients = this->webSocketServer->getConnectedClients();
        fmt::print("Connected clients: {}\n", connectedClients);
        fmt::print("Active sessions: {}\n", this->sessions.size());
        this->lastStatsTime = now;
    }
}

