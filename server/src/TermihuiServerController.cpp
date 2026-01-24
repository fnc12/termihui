#include "TermihuiServerController.h"
#include "JsonHelper.h"
#include <termihui/protocol/protocol.h>
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
    
    // AI agent will be configured per-request based on selected provider
    fmt::print("🤖 AI Agent ready (provider configured per-request)\n");
    
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
            StatusMessage statusMessage{sessionId, false};
            this->webSocketServer->broadcastMessage(serialize(statusMessage));
        }
    }
    
    // Process AI agent events
    auto aiEvents = this->aiAgentController.update();
    for (auto& event : aiEvents) {
        switch (event.type) {
            case AIEvent::Type::Chunk:
                this->webSocketServer->broadcastMessage(serialize(AIChunkMessage{event.sessionId, std::move(event.content)}));
                break;
            case AIEvent::Type::Done:
                this->webSocketServer->broadcastMessage(serialize(AIDoneMessage{event.sessionId}));
                break;
            case AIEvent::Type::Error:
                this->webSocketServer->broadcastMessage(serialize(AIErrorMessage{event.sessionId, std::move(event.content)}));
                break;
        }
    }
    
    // Print statistics every 30 seconds
    this->printStats();
    
    // Small pause to avoid CPU overload
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void TermihuiServerController::handleNewConnection(int clientId) {
    fmt::print("Client connected: {}\n", clientId);
    
    ConnectedMessage connectedMessage;
    connectedMessage.serverVersion = "1.0.0";
    // Add home directory for path shortening on client
    if (const char* home = getenv("HOME")) {
        connectedMessage.home = home;
    }
    this->webSocketServer->sendMessage(clientId, serialize(connectedMessage));
    
    // Client will request session list and history separately
}

void TermihuiServerController::handleDisconnection(int clientId) {
    fmt::print("Client disconnected: {}\n", clientId);
}

void TermihuiServerController::handleMessage(const WebSocketServer::IncomingMessage& incomingMessage) {
    fmt::print("Processing message from {}: {}\n", incomingMessage.clientId, incomingMessage.text);
    
    try {
        ClientMessage clientMessage = parseClientMessage(incomingMessage.text);
        
        std::visit([this, clientId = incomingMessage.clientId](const auto& message) {
            this->handleMessageFromClient(clientId, message);
        }, clientMessage);
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "Message parsing error: {}\n", e.what());
        ErrorMessage errorMessage{std::string("Invalid message: ") + e.what(), "PARSE_ERROR"};
        this->webSocketServer->sendMessage(incomingMessage.clientId, serialize(errorMessage));
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

void TermihuiServerController::handleMessageFromClient(int clientId, const ExecuteMessage& message) {
    auto* terminalSessionController = this->findSession(message.sessionId);
    if (!terminalSessionController) {
        ErrorMessage errorMessage{fmt::format("Session {} not found", message.sessionId), "SESSION_NOT_FOUND"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        return;
    }
    
    terminalSessionController->setPendingCommand(message.command);
    
    using ExecuteCommandResult = TerminalSessionController::ExecuteCommandResult;
    const ExecuteCommandResult executeCommandResult = terminalSessionController->executeCommand(message.command);
    
    if (executeCommandResult.isOk()) {
        fmt::print("Session {}: Executed command: {}\n", message.sessionId, message.command);
    } else {
        ErrorMessage errorMessage{fmt::format("Failed to execute command *{}*: {}", message.command, executeCommandResult.errorText()), "COMMAND_FAILED"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
    }
}

void TermihuiServerController::handleMessageFromClient(int clientId, const InputMessage& message) {
    auto* terminalSessionController = this->findSession(message.sessionId);
    if (!terminalSessionController) {
        ErrorMessage errorMessage{fmt::format("Session {} not found", message.sessionId), "SESSION_NOT_FOUND"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        return;
    }
    
    ssize_t bytes = terminalSessionController->sendInput(message.text);
    if (bytes >= 0) {
        InputSentMessage inputSentMessage{static_cast<int>(bytes)};
        this->webSocketServer->sendMessage(clientId, serialize(inputSentMessage));
    } else {
        ErrorMessage errorMessage{"Failed to send input", "INPUT_FAILED"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
    }
}
            
void TermihuiServerController::handleMessageFromClient(int clientId, const CompletionMessage& message) {
    fmt::print("Completion request for session {}: '{}' (position: {})\n", message.sessionId, message.text, message.cursorPosition);
    
    auto* terminalSessionController = this->findSession(message.sessionId);
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
    
    auto completions = this->completionManager.getCompletions(message.text, message.cursorPosition, currentDir);
    
    CompletionResultMessage completionResultMessage{
        std::move(completions),
        message.text,
        message.cursorPosition
    };
    this->webSocketServer->sendMessage(clientId, serialize(completionResultMessage));
}

void TermihuiServerController::handleMessageFromClient(int clientId, const ResizeMessage& message) {
    if (message.cols <= 0 || message.rows <= 0) {
        ErrorMessage errorMessage{"Invalid terminal size", "INVALID_SIZE"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        return;
    }
    
    auto* terminalSessionController = this->findSession(message.sessionId);
    if (!terminalSessionController) {
        ErrorMessage errorMessage{fmt::format("Session {} not found", message.sessionId), "SESSION_NOT_FOUND"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        return;
    }
    
    if (terminalSessionController->setWindowSize(static_cast<unsigned short>(message.cols), static_cast<unsigned short>(message.rows))) {
        ResizeAckMessage resizeAckMessage{message.cols, message.rows};
        this->webSocketServer->sendMessage(clientId, serialize(resizeAckMessage));
    } else {
        ErrorMessage errorMessage{"Failed to set terminal size", "RESIZE_FAILED"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
    }
}

void TermihuiServerController::handleMessageFromClient(int clientId, const ListSessionsMessage&) {
    auto storageSessions = this->serverStorage->getActiveTerminalSessions();
    
    SessionsListMessage sessionsListMessage;
    sessionsListMessage.sessions.reserve(storageSessions.size());
    for (const auto& s : storageSessions) {
        sessionsListMessage.sessions.push_back(SessionInfo{s.id, s.createdAt});
    }
    
    this->webSocketServer->sendMessage(clientId, serialize(sessionsListMessage));
    fmt::print("Sent sessions list ({} sessions) to client {}\n", storageSessions.size(), clientId);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const CreateSessionMessage&) {
    // Create session record in DB
    uint64_t sessionId = this->serverStorage->createTerminalSession(this->currentRunId);
    
    // Create terminal session controller
    auto sessionDbPath = this->fileSystemManager.getWritablePath() / fmt::format("session_{}.sqlite", sessionId);
    auto controller = std::make_unique<TerminalSessionController>(
        sessionDbPath, sessionId, this->currentRunId);
    
    if (!controller->createSession()) {
        ErrorMessage errorMessage{"Failed to create terminal session", "SESSION_CREATE_FAILED"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        // TODO: cleanup DB record
        return;
    }
    
    this->sessions[sessionId] = std::move(controller);
    
    SessionCreatedMessage sessionCreatedMessage{sessionId};
    this->webSocketServer->sendMessage(clientId, serialize(sessionCreatedMessage));
    fmt::print("Created session {} for client {}\n", sessionId, clientId);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const CloseSessionMessage& message) {
    auto it = this->sessions.find(message.sessionId);
    if (it == this->sessions.end()) {
        ErrorMessage errorMessage{fmt::format("Session {} not found", message.sessionId), "SESSION_NOT_FOUND"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        return;
    }
    
    // Terminate and remove session
    it->second->terminate();
    this->sessions.erase(it);
    
    // Mark as deleted in DB
    this->serverStorage->markTerminalSessionAsDeleted(message.sessionId);
    
    SessionClosedMessage sessionClosedMessage{message.sessionId};
    this->webSocketServer->sendMessage(clientId, serialize(sessionClosedMessage));
    fmt::print("Closed session {} for client {}\n", message.sessionId, clientId);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const GetHistoryMessage& message) {
    auto* terminalSessionController = this->findSession(message.sessionId);
    if (!terminalSessionController) {
        ErrorMessage errorMessage{fmt::format("Session {} not found", message.sessionId), "SESSION_NOT_FOUND"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        return;
    }
    
    auto commandHistory = terminalSessionController->getCommandHistory();
    
    HistoryMessage historyMessage;
    historyMessage.sessionId = message.sessionId;
    historyMessage.commands.reserve(commandHistory.size());
    for (const auto& record : commandHistory) {
        historyMessage.commands.push_back(CommandRecord{
            record.id,
            record.command,
            this->outputParser.parse(record.output),
            record.exitCode,
            record.cwdStart,
            record.cwdEnd,
            record.isFinished
        });
    }
    
    this->webSocketServer->sendMessage(clientId, serialize(historyMessage));
    fmt::print("Sent history for session {} ({} commands) to client {}\n", message.sessionId, commandHistory.size(), clientId);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const AIChatMessage& message) {
    fmt::print("AI chat message for session {}, provider {}: {}\n", message.sessionId, message.providerId, message.message);
    
    // Get provider from storage
    auto provider = this->serverStorage->getLLMProvider(message.providerId);
    if (!provider) {
        ErrorMessage errorMessage{fmt::format("LLM provider {} not found", message.providerId), "PROVIDER_NOT_FOUND"};
        this->webSocketServer->sendMessage(clientId, serialize(errorMessage));
        return;
    }
    
    // Configure AI agent with provider settings
    this->aiAgentController.setEndpoint(provider->url);
    this->aiAgentController.setModel(provider->model);
    this->aiAgentController.setApiKey(provider->apiKey);
    
    // Send message to AI agent (will respond with streaming events)
    this->aiAgentController.sendMessage(message.sessionId, message.message);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const ListLLMProvidersMessage&) {
    auto providers = this->serverStorage->getAllLLMProviders();
    
    LLMProvidersListMessage response;
    response.providers.reserve(providers.size());
    for (const auto& p : providers) {
        response.providers.push_back(LLMProviderInfo{
            p.id, p.name, p.type, p.url, p.model, p.createdAt
        });
    }
    
    this->webSocketServer->sendMessage(clientId, serialize(response));
    fmt::print("Sent LLM providers list ({} providers) to client {}\n", providers.size(), clientId);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const AddLLMProviderMessage& message) {
    uint64_t id = this->serverStorage->addLLMProvider(
        message.name, message.providerType, message.url, message.model, message.apiKey);
    
    LLMProviderAddedMessage response{id};
    this->webSocketServer->sendMessage(clientId, serialize(response));
    fmt::print("Added LLM provider {} (id={}) for client {}\n", message.name, id, clientId);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const UpdateLLMProviderMessage& message) {
    this->serverStorage->updateLLMProvider(message.id, message.name, message.url, message.model, message.apiKey);
    
    LLMProviderUpdatedMessage response{message.id};
    this->webSocketServer->sendMessage(clientId, serialize(response));
    fmt::print("Updated LLM provider {} for client {}\n", message.id, clientId);
}

void TermihuiServerController::handleMessageFromClient(int clientId, const DeleteLLMProviderMessage& message) {
    this->serverStorage->deleteLLMProvider(message.id);
    
    LLMProviderDeletedMessage response{message.id};
    this->webSocketServer->sendMessage(clientId, serialize(response));
    fmt::print("Deleted LLM provider {} for client {}\n", message.id, clientId);
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
                    this->webSocketServer->broadcastMessage(serialize(OutputMessage{this->outputParser.parse(chunk)}));
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
                this->webSocketServer->broadcastMessage(serialize(OutputMessage{this->outputParser.parse(chunk)}));
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
                this->webSocketServer->broadcastMessage(serialize(OutputMessage{this->outputParser.parse(chunk)}));
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
                CommandStartMessage commandStartMessage;
                if (!cwd.empty()) {
                    commandStartMessage.cwd = cwd;
                }
                this->webSocketServer->broadcastMessage(serialize(commandStartMessage));
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
                
                CommandEndMessage commandEndMessage;
                commandEndMessage.exitCode = exitCode;
                if (!cwd.empty()) {
                    commandEndMessage.cwd = cwd;
                }
                this->webSocketServer->broadcastMessage(serialize(commandEndMessage));
            }
        } else if (osc.rfind("\x1b]133;C", 0) == 0) {
            // OSC 133;C - Prompt start
            fmt::print("[OSC-PARSE] >>> OSC 133;C (prompt_start)\n");
            this->webSocketServer->broadcastMessage(serialize(PromptStartMessage{}));
        } else if (osc.rfind("\x1b]133;D", 0) == 0) {
            // OSC 133;D - Prompt end
            fmt::print("[OSC-PARSE] >>> OSC 133;D (prompt_end)\n");
            this->webSocketServer->broadcastMessage(serialize(PromptEndMessage{}));
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
                    this->webSocketServer->broadcastMessage(serialize(CwdUpdateMessage{path}));
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
                        this->webSocketServer->broadcastMessage(serialize(CwdUpdateMessage{path}));
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
