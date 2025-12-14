#include "TerminalSession.h"
#include "WebSocketServer.h"
#include "JsonHelper.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <sstream>
#include "hv/json.hpp"
#include "hv/hlog.h"
#include <fmt/core.h>

std::atomic<bool> shouldExit{false};

// Structure for storing command history
struct CommandRecord {
    std::string command;
    std::string output;
    int exitCode = 0;
    std::string cwdStart;
    std::string cwdEnd;
    bool isFinished = false;
};

// Command history (persisted while server is running)
std::vector<CommandRecord> commandHistory;
int currentCommandIndex = -1; // Index of the current unfinished command
std::string pendingCommand;   // Command waiting for command_start event

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        shouldExit = true;
    }
}

using json = nlohmann::json;

int main(int argc, char* argv[])
{
    // Disable automatic libhv logs
    hlog_disable();
    
    // Set up signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    fmt::print("=== TermiHUI Server ===\n");
    fmt::print("Server ready\n");
    fmt::print("Port: 37854 (WebSocket)\n");
    fmt::print("Press Ctrl+C to stop\n\n");
    
    // Create single terminal session
    auto terminalSession = std::make_unique<TerminalSession>();
    
    // Create interactive bash session
    if (!terminalSession->createSession()) {
        fmt::print(stderr, "Failed to create terminal session\n");
        return 1;
    }
    
    // Create and start WebSocket server
    WebSocketServer webSocketServer(37854);
    if (!webSocketServer.start()) {
        fmt::print(stderr, "Failed to start WebSocket server\n");
        return 1;
    }
    
    fmt::print("Server started! Waiting for connections...\n\n");
    
    // State tracking for main loop
    bool wasRunning = false;
    auto lastStatsTime = std::chrono::steady_clock::now();
    
    // Main server loop
    while (!shouldExit) {
        // Get events from WebSocket server
        std::vector<WebSocketServer::IncomingMessage> incomingMessages;
        std::vector<WebSocketServer::ConnectionEvent> connectionEvents;
        
        webSocketServer.update(incomingMessages, connectionEvents);
        
        // Handle connection/disconnection events
        for (const auto& event : connectionEvents) {
            if (event.connected) {
                fmt::print("Client connected: {}\n", event.clientId);
                // Send welcome message with current cwd
                json welcomeMsg;
                welcomeMsg["type"] = "connected";
                welcomeMsg["server_version"] = "1.0.0";
                std::string cwd = terminalSession->getLastKnownCwd();
                if (!cwd.empty()) {
                    welcomeMsg["cwd"] = cwd;
                }
                webSocketServer.sendMessage(event.clientId, welcomeMsg.dump());
                
                // Send command history
                if (!commandHistory.empty()) {
                    json historyMsg;
                    historyMsg["type"] = "history";
                    json commands = json::array();
                    for (const auto& record : commandHistory) {
                        json cmd;
                        cmd["command"] = record.command;
                        cmd["output"] = record.output;
                        cmd["exit_code"] = record.exitCode;
                        cmd["cwd_start"] = record.cwdStart;
                        cmd["cwd_end"] = record.cwdEnd;
                        cmd["is_finished"] = record.isFinished;
                        commands.push_back(cmd);
                    }
                    historyMsg["commands"] = commands;
                    webSocketServer.sendMessage(event.clientId, historyMsg.dump());
                    fmt::print("Sent history: {} commands\n", commandHistory.size());
                }
            } else {
                fmt::print("Client disconnected: {}\n", event.clientId);
            }
        }
        
        // Handle incoming messages
        for (const auto& msg : incomingMessages) {
            fmt::print("Processing message from {}: {}\n", msg.clientId, msg.message);
            
            try {
                json msgJson = json::parse(msg.message);
                std::string type = msgJson.value("type", "");
                
                if (type == "execute") {
                    std::string command = msgJson.value("command", "");
                    if (!command.empty()) {
                        // Save command for history recording on command_start
                        pendingCommand = command;
                        
                        if (terminalSession->executeCommand(command)) {
                            fmt::print("Executed command: {}\n", command);
                        } else {
                            std::string error = JsonHelper::createResponse("error", "Failed to execute command");
                            webSocketServer.sendMessage(msg.clientId, error);
                        }
                    } else {
                        std::string error = JsonHelper::createResponse("error", "Missing command field");
                        webSocketServer.sendMessage(msg.clientId, error);
                    }
                } else if (type == "input") {
                    std::string text = msgJson.value("text", "");
                    if (!text.empty()) {
                        ssize_t bytes = terminalSession->sendInput(text);
                        if (bytes >= 0) {
                            std::string response = JsonHelper::createResponse("input_sent", "", bytes);
                            webSocketServer.sendMessage(msg.clientId, response);
                        } else {
                            std::string error = JsonHelper::createResponse("error", "Failed to send input");
                            webSocketServer.sendMessage(msg.clientId, error);
                        }
                    } else {
                        std::string error = JsonHelper::createResponse("error", "Missing text field");
                        webSocketServer.sendMessage(msg.clientId, error);
                    }
                } else if (type == "completion") {
                    std::string text = msgJson.value("text", "");
                    int cursorPosition = msgJson.value("cursor_position", 0);
                    
                    fmt::print("Completion request: '{}' (position: {})\n", text, cursorPosition);
                    
                    // Get completion options
                    auto completions = terminalSession->getCompletions(text, cursorPosition);
                    
                    // Create JSON response with options
                    json response;
                    response["type"] = "completion_result";
                    response["completions"] = completions;
                    response["original_text"] = text;
                    response["cursor_position"] = cursorPosition;
                    
                    webSocketServer.sendMessage(msg.clientId, response.dump());
                } else if (type == "resize") {
                    int cols = msgJson.value("cols", 80);
                    int rows = msgJson.value("rows", 24);
                    
                    if (cols > 0 && rows > 0) {
                        if (terminalSession->setWindowSize(static_cast<unsigned short>(cols), static_cast<unsigned short>(rows))) {
                            json response;
                            response["type"] = "resize_ack";
                            response["cols"] = cols;
                            response["rows"] = rows;
                            webSocketServer.sendMessage(msg.clientId, response.dump());
                        } else {
                            std::string error = JsonHelper::createResponse("error", "Failed to set terminal size");
                            webSocketServer.sendMessage(msg.clientId, error);
                        }
                    } else {
                        std::string error = JsonHelper::createResponse("error", "Invalid terminal size");
                        webSocketServer.sendMessage(msg.clientId, error);
                    }
                } else {
                    std::string error = JsonHelper::createResponse("error", "Unknown message type");
                    webSocketServer.sendMessage(msg.clientId, error);
                }
            } catch (const json::exception& e) {
                fmt::print(stderr, "JSON parsing error: {}\n", e.what());
                std::string error = JsonHelper::createResponse("error", "Invalid JSON format");
                webSocketServer.sendMessage(msg.clientId, error);
            }
        }
        
        // Check terminal session output
        if (terminalSession->hasData(0)) {
            std::string output = terminalSession->readOutput();
            if (!output.empty()) {
                // Streaming parser: preserve order "text → event → text"
                size_t i = 0;
                while (true) {
                    size_t oscPos = output.find("\x1b]133;", i);
                    if (oscPos == std::string::npos) {
                        // Remainder as regular output
                        if (i < output.size()) {
                            std::string chunk = output.substr(i);
                            if (!chunk.empty()) {
                                fmt::print("Terminal output: *{}*\n", chunk);
                                // Add to current history record
                                if (currentCommandIndex >= 0 && currentCommandIndex < static_cast<int>(commandHistory.size())) {
                                    commandHistory[currentCommandIndex].output += chunk;
                                }
                                webSocketServer.broadcastMessage(JsonHelper::createResponse("output", chunk));
                            }
                        }
                        break;
                    }

                    // Send text before marker
                    if (oscPos > i) {
                        std::string chunk = output.substr(i, oscPos - i);
                        if (!chunk.empty()) {
                            fmt::print("Terminal output: *{}*\n", chunk);
                            // Add to current history record
                            if (currentCommandIndex >= 0 && currentCommandIndex < static_cast<int>(commandHistory.size())) {
                                commandHistory[currentCommandIndex].output += chunk;
                            }
                            webSocketServer.broadcastMessage(JsonHelper::createResponse("output", chunk));
                        }
                    }

                    // Find OSC end (BEL)
                    size_t oscEnd = output.find('\x07', oscPos);
                    if (oscEnd == std::string::npos) {
                        // Incomplete marker — treat rest as regular text
                        std::string chunk = output.substr(oscPos);
                        if (!chunk.empty()) {
                            fmt::print("Terminal output: *{}*\n", chunk);
                            // Add to current history record
                            if (currentCommandIndex >= 0 && currentCommandIndex < static_cast<int>(commandHistory.size())) {
                                commandHistory[currentCommandIndex].output += chunk;
                            }
                            webSocketServer.broadcastMessage(JsonHelper::createResponse("output", chunk));
                        }
                        break;
                    }

                    std::string osc = output.substr(oscPos, oscEnd - oscPos + 1);
                    
                    // Helper lambda to extract parameter value from OSC string
                    auto extractParam = [&osc](const std::string& key) -> std::string {
                        auto pos = osc.find(key + "=");
                        if (pos == std::string::npos) return "";
                        size_t start = pos + key.length() + 1;
                        size_t end = osc.find_first_of(";\x07", start);
                        if (end == std::string::npos) end = osc.length();
                        return osc.substr(start, end - start);
                    };
                    
                    // Process event
                    if (osc.rfind("\x1b]133;A", 0) == 0) {
                        std::string cwd = extractParam("cwd");
                        if (!cwd.empty()) {
                            terminalSession->setLastKnownCwd(cwd); // Update cwd for tab completion
                        }
                        
                        // Create record and send event only if there's a pending command
                        // Otherwise it's just shell prompt on startup
                        if (!pendingCommand.empty()) {
                            json ev;
                            ev["type"] = "command_start";
                            if (!cwd.empty()) {
                                ev["cwd"] = cwd;
                            }
                            
                            // Create new history record
                            CommandRecord record;
                            record.command = pendingCommand;
                            record.cwdStart = cwd;
                            commandHistory.push_back(record);
                            currentCommandIndex = static_cast<int>(commandHistory.size()) - 1;
                            pendingCommand.clear();
                            
                            webSocketServer.broadcastMessage(ev.dump());
                        }
                    } else if (osc.rfind("\x1b]133;B", 0) == 0) {
                        int exitCode = 0;
                        auto k = osc.find("exit=");
                        if (k != std::string::npos) exitCode = std::atoi(osc.c_str() + k + 5);
                        std::string cwd = extractParam("cwd");
                        if (!cwd.empty()) {
                            terminalSession->setLastKnownCwd(cwd); // Update cwd for tab completion
                        }
                        
                        // Finish current history record and send event
                        // only if there's an active block
                        if (currentCommandIndex >= 0 && currentCommandIndex < static_cast<int>(commandHistory.size())) {
                            commandHistory[currentCommandIndex].exitCode = exitCode;
                            commandHistory[currentCommandIndex].cwdEnd = cwd;
                            commandHistory[currentCommandIndex].isFinished = true;
                            currentCommandIndex = -1; // Reset pointer
                            
                            json ev;
                            ev["type"] = "command_end";
                            ev["exit_code"] = exitCode;
                            if (!cwd.empty()) {
                                ev["cwd"] = cwd;
                            }
                            webSocketServer.broadcastMessage(ev.dump());
                        }
                    } else if (osc.rfind("\x1b]133;C", 0) == 0) {
                        json ev; ev["type"] = "prompt_start"; webSocketServer.broadcastMessage(ev.dump());
                    } else if (osc.rfind("\x1b]133;D", 0) == 0) {
                        json ev; ev["type"] = "prompt_end"; webSocketServer.broadcastMessage(ev.dump());
                    }

                    // Continue after marker
                    i = oscEnd + 1;
                }
            }
        }
        
        // Check session status and send completion notification
        bool currentlyRunning = terminalSession->isRunning();
        if (wasRunning && !currentlyRunning) {
            fmt::print("Command completed\n");
            std::string status = JsonHelper::createResponse("status", "", 0, false);
            webSocketServer.broadcastMessage(status);
        }
        wasRunning = currentlyRunning;
        
        // Print statistics every 30 seconds
        auto now = std::chrono::steady_clock::now();
        if (now - lastStatsTime > std::chrono::seconds(30)) {
            size_t connectedClients = webSocketServer.getConnectedClients();
            fmt::print("Connected clients: {}\n", connectedClients);
            fmt::print("Terminal session active: {}\n", (terminalSession->isRunning() ? "yes" : "no"));
            lastStatsTime = now;
        }
        
        // Small pause to avoid CPU overload
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Reduced to 10ms for better responsiveness
    }
    
    fmt::print("\n=== Server shutdown ===\n");
    webSocketServer.stop();
    fmt::print("Server stopped\n");
    return 0;
}

// TODO: Future improvements:
// 1. Implement resize event handling from client
// 2. Add support for multiple sessions (tabs)
// 3. Integrate with AI agent for command and output analysis
// 4. Add authentication and authorization
// 5. Implement logging of all operations
// 6. Add security settings (chroot, command restrictions)
// 7. Support for command chains with sudo
// 8. Integration with monitoring system
// 9. Add support for persistent command history 