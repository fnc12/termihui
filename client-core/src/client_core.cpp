#include "termihui/client_core.h"
#include "termihui/client_core_c.h"
#include "termihui/ansi_parser.h"
#include "termihui/websocket_client_controller.h"
#include "termihui/client_storage.h"
#include <termihui/protocol/protocol.h>
#include <fmt/core.h>
#include <thread>
#include <hv/json.hpp>
#include <sago/platform_folders.h>


// Storage keys
static constexpr const char* KEY_LAST_SESSION_ID = "last_session_id";

using json = nlohmann::json;

namespace termihui {

// ADL to_json for Color
void to_json(json& j, const Color& color) {
    switch (color.type) {
        case Color::Type::Standard:
            j = std::string(colorName(color.index));
            break;
        case Color::Type::Bright:
            j = std::string("bright_") + std::string(colorName(color.index));
            break;
        case Color::Type::Indexed:
            j = json{{"index", color.index}};
            break;
        case Color::Type::RGB:
            j = json{{"rgb", fmt::format("#{:02X}{:02X}{:02X}", color.r, color.g, color.b)}};
            break;
    }
}

// ADL to_json for TextStyle
void to_json(json& j, const TextStyle& style) {
    j["fg"] = style.fg ? json(*style.fg) : json(nullptr);
    j["bg"] = style.bg ? json(*style.bg) : json(nullptr);
    j["bold"] = style.bold;
    j["dim"] = style.dim;
    j["italic"] = style.italic;
    j["underline"] = style.underline;
    j["reverse"] = style.reverse;
    j["strikethrough"] = style.strikethrough;
}

// ADL to_json for StyledSegment
void to_json(json& j, const StyledSegment& segment) {
    j = json{
        {"text", segment.text},
        {"style", segment.style}
    };
}

// Version
static constexpr const char* VERSION = "1.0.0";

// Static instance
ClientCoreController ClientCoreController::instance;

// ClientCoreController implementation

ClientCoreController::ClientCoreController()
    : webSocketController(std::make_unique<WebSocketClientController>())
    , ansiParser(std::make_unique<ANSIParser>()) {
}

ClientCoreController::~ClientCoreController() {
    if (this->initialized) {
        this->shutdown();
    }
}

const char* ClientCoreController::getVersion() {
    return VERSION;
}

bool ClientCoreController::initialize() {
    if (this->initialized) {
        fmt::print(stderr, "ClientCoreController: Already initialized\n");
        return false;
    }
    
    fmt::print("ClientCoreController: Initializing v{}\n", VERSION);
    
    // Initialize persistent storage
    std::filesystem::path storagePath = sago::getDataHome();
    storagePath /= "termihui";
    std::filesystem::create_directories(storagePath);
    storagePath /= "client_state.sqlite";
    
    this->clientStorage = std::make_unique<ClientStorage>(storagePath);
    fmt::print("ClientCoreController: Storage initialized at {}\n", storagePath.string());
    
    // Load last active session
    auto lastSessionId = this->clientStorage->getUInt64(KEY_LAST_SESSION_ID);
    if (lastSessionId) {
        this->activeSessionId = *lastSessionId;
        fmt::print("ClientCoreController: Restored last session ID: {}\n", this->activeSessionId);
    }
    
    this->initialized = true;
    fmt::print("ClientCoreController: Initialized successfully\n");
    
    return true;
}

void ClientCoreController::shutdown() {
    if (!this->initialized) {
        return;
    }
    
    fmt::print("ClientCoreController: Shutting down\n");
    
    // Close WebSocket connection
    if (this->webSocketController) {
        this->webSocketController->close();
    }
    
    this->pendingEvents.clear();
    this->lastResponse.clear();
    this->lastEvent.clear();
    this->serverAddress.clear();
    this->activeSessionId = 0;
    this->initialized = false;
}

std::string ClientCoreController::sendMessage(std::string_view message) {
    auto setResponse = [this](std::string result) -> std::string {
        this->lastResponse = result;
        return result;
    };
    
    if (!this->initialized) {
        return setResponse("Not initialized");
    }
    
    // DEBUG: Print thread id
    fmt::print("ClientCoreController::sendMessage [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()), 
               message);
    
    // Parse JSON message
    try {
        auto j = json::parse(message);
        std::string type = j.at("type").get<std::string>();
        
        if (type == "connectButtonClicked") {
            return setResponse(this->handleConnectButtonClicked(j.at("address").get<std::string>()));
        } else if (type == "requestReconnect") {
            return setResponse(this->handleRequestReconnect(j.at("address").get<std::string>()));
        } else if (type == "disconnectButtonClicked") {
            return setResponse(this->handleDisconnectButtonClicked());
        } else if (type == "executeCommand") {
            return setResponse(this->handleExecuteCommand(j.at("command").get<std::string>()));
        } else if (type == "sendInput") {
            return setResponse(this->handleSendInput(j.at("text").get<std::string>()));
        } else if (type == "resize") {
            return setResponse(this->handleResize(j.at("cols").get<int>(), j.at("rows").get<int>()));
        } else if (type == "requestCompletion") {
            return setResponse(this->handleRequestCompletion(j.at("text").get<std::string>(), j.at("cursorPosition").get<int>()));
        } else if (type == "createSession") {
            return setResponse(this->handleCreateSession());
        } else if (type == "closeSession") {
            return setResponse(this->handleCloseSession(j.at("sessionId").get<uint64_t>()));
        } else if (type == "switchSession") {
            return setResponse(this->handleSwitchSession(j.at("sessionId").get<uint64_t>()));
        } else if (type == "listSessions") {
            return setResponse(this->handleListSessions());
        } else {
            return setResponse(fmt::format("Unknown message type: {}", type));
        }
    } catch (const std::exception& e) {
        return setResponse(fmt::format("Failed to parse message: {}", e.what()));
    } catch (...) {
        return setResponse("unknown exception");
    }
}

const char* ClientCoreController::pollEvent() {
    auto event = this->pendingEvents.pop();
    if (!event.has_value()) {
        return nullptr;
    }
    
    // Store in buffer for C API stability
    this->lastEvent = std::move(event.value());
    return this->lastEvent.c_str();
}

void ClientCoreController::pushEvent(std::string event) {
    // DEBUG: Print thread id
    fmt::print("ClientCoreController::pushEvent [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()), 
               event);
    
    this->pendingEvents.push(std::move(event));
}

size_t ClientCoreController::pendingEventsCount() const {
    return this->pendingEvents.size();
}

// Message handlers

std::string ClientCoreController::handleConnectButtonClicked(std::string_view address) {
    fmt::print("ClientCoreController: Connect button clicked, address: {}\n", address);
    
    if (address.empty()) {
        return "Address is empty";
    }
    
    this->serverAddress = std::string(address);
    
    // Build WebSocket URL
    std::string wsUrl = "ws://" + std::string(address);
    
    // Notify UI that we're connecting
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "connecting"},
        {"address", address}
    }.dump());
    
    // Connect (callbacks are handled via update() now)
    fmt::print("ClientCoreController: Connecting to {}\n", wsUrl);
    int ret = this->webSocketController->open(wsUrl);
    if (ret != 0) {
        return fmt::format("Failed to initiate connection {}", ret);
    }
    
    return "";
}

std::string ClientCoreController::handleRequestReconnect(std::string_view address) {
    fmt::print("ClientCoreController: Auto-reconnect requested, address: {}\n", address);
    
    if (address.empty()) {
        return "Address is empty";
    }
    
    this->serverAddress = std::string(address);
    std::string wsUrl = "ws://" + std::string(address);
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "connecting"},
        {"address", address}
    }.dump());
    
    fmt::print("ClientCoreController: Reconnecting to {}\n", wsUrl);
    int ret = this->webSocketController->open(wsUrl);
    if (ret != 0) {
        return fmt::format("Failed to initiate connection {}", ret);
    }
    
    return "";
}

std::string ClientCoreController::handleDisconnectButtonClicked() {
    fmt::print("ClientCoreController: Disconnect button clicked\n");
    
    if (this->webSocketController) {
        this->webSocketController->close();
    }
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "disconnected"}
    }.dump());
    
    return "";
}

std::string ClientCoreController::handleExecuteCommand(std::string_view command) {
    fmt::print("ClientCoreController: Execute command: {}\n", command);
    
    if (!this->webSocketController || !this->webSocketController->isConnected()) {
        return "Not connected to server";
    }
    
    if (this->activeSessionId == 0) {
        return "No active session";
    }
    
    // Save for block header
    this->lastSentCommand.set(std::string(command));
    
    ExecuteMessage message{this->activeSessionId, std::string(command)};
    this->webSocketController->send(serialize(message));
    
    return "";
}

std::string ClientCoreController::handleSendInput(std::string_view text) {
    fmt::print("ClientCoreController: Send input: {}\n", text.substr(0, 20));
    
    if (!this->webSocketController || !this->webSocketController->isConnected()) {
        return "Not connected to server";
    }
    
    if (this->activeSessionId == 0) {
        return "No active session";
    }
    
    InputMessage message{this->activeSessionId, std::string(text)};
    this->webSocketController->send(serialize(message));
    
    return "";
}

std::string ClientCoreController::handleResize(int cols, int rows) {
    fmt::print("ClientCoreController: Resize: {}x{}\n", cols, rows);
    
    if (!this->webSocketController || !this->webSocketController->isConnected()) {
        return "Not connected to server";
    }
    
    if (this->activeSessionId == 0) {
        return "No active session";
    }
    
    ResizeMessage message{this->activeSessionId, cols, rows};
    this->webSocketController->send(serialize(message));
    
    return "";
}

std::string ClientCoreController::handleRequestCompletion(std::string_view text, int cursorPosition) {
    fmt::print("ClientCoreController: Request completion for: '{}' at {}\n", text, cursorPosition);
    
    if (!this->webSocketController || !this->webSocketController->isConnected()) {
        return "Not connected to server";
    }
    
    if (this->activeSessionId == 0) {
        return "No active session";
    }
    
    CompletionMessage message{this->activeSessionId, std::string(text), cursorPosition};
    this->webSocketController->send(serialize(message));
    
    return "";
}

// Session management handlers

std::string ClientCoreController::handleCreateSession() {
    fmt::print("ClientCoreController: Create session\n");
    
    if (!this->webSocketController || !this->webSocketController->isConnected()) {
        return "Not connected to server";
    }
    
    this->webSocketController->send(serialize(CreateSessionMessage{}));
    
    return "";
}

std::string ClientCoreController::handleCloseSession(uint64_t sessionId) {
    fmt::print("ClientCoreController: Close session {}\n", sessionId);
    
    if (!this->webSocketController || !this->webSocketController->isConnected()) {
        return "Not connected to server";
    }
    
    this->webSocketController->send(serialize(CloseSessionMessage{sessionId}));
    
    return "";
}

std::string ClientCoreController::handleSwitchSession(uint64_t sessionId) {
    fmt::print("ClientCoreController: Switch to session {}\n", sessionId);
    
    this->activeSessionId = sessionId;
    
    // Persist last session ID
    if (this->clientStorage) {
        this->clientStorage->setUInt64(KEY_LAST_SESSION_ID, sessionId);
    }
    
    // Request history for new session
    if (this->webSocketController && this->webSocketController->isConnected()) {
        this->webSocketController->send(serialize(GetHistoryMessage{sessionId}));
    }
    
    return "";
}

std::string ClientCoreController::handleListSessions() {
    fmt::print("ClientCoreController: List sessions\n");
    
    if (!this->webSocketController || !this->webSocketController->isConnected()) {
        return "Not connected to server";
    }
    
    this->webSocketController->send(serialize(ListSessionsMessage{}));
    
    return "";
}

// Main update tick

void ClientCoreController::update() {
    if (!this->webSocketController) {
        return;
    }
    
    using WsCtrl = WebSocketClientController;
    
    auto events = this->webSocketController->update();
    for (auto& event : events) {
        std::visit([this](auto&& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, WsCtrl::OpenEvent>) {
                this->onWebSocketOpen();
            } else if constexpr (std::is_same_v<T, WsCtrl::MessageEvent>) {
                this->onWebSocketMessage(e.message);
            } else if constexpr (std::is_same_v<T, WsCtrl::CloseEvent>) {
                this->onWebSocketClose();
            } else if constexpr (std::is_same_v<T, WsCtrl::ErrorEvent>) {
                this->onWebSocketError(e.error);
            }
        }, event);
    }
}

// WebSocket event handlers (called from main thread via update())

void ClientCoreController::onWebSocketOpen() {
    fmt::print("ClientCoreController::onWebSocketOpen [thread:{}]\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "connected"},
        {"address", this->serverAddress}
    }.dump());
    
    // Request sessions list on connect
    this->webSocketController->send(serialize(ListSessionsMessage{}));
    fmt::print("ClientCoreController: Requested sessions list\n");
}

void ClientCoreController::onWebSocketMessage(const std::string& message) {
    fmt::print("ClientCoreController::onWebSocketMessage [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()),
               message.substr(0, 100)); // Truncate for debug
    
    // Forward message to UI as event
    try {
        auto serverData = json::parse(message);
        std::string_view messageType = serverData.at("type").get<std::string_view>();
        
        // Handle sessions_list - log and create if empty
        if (messageType == "sessions_list") {
            auto& sessions = serverData.at("sessions");
            fmt::print("ClientCoreController: Received {} sessions:\n", sessions.size());
            for (const auto& s : sessions) {
                fmt::print("  - Session ID: {}\n", s.at("id").get<uint64_t>());
            }
            if (sessions.empty()) {
                fmt::print("ClientCoreController: No sessions, creating new one\n");
                this->webSocketController->send(serialize(CreateSessionMessage{}));
            } else {
                // Try to restore last session if it exists in the list
                uint64_t selectedId = 0;
                if (this->activeSessionId != 0) {
                    for (const auto& s : sessions) {
                        if (s.at("id").get<uint64_t>() == this->activeSessionId) {
                            selectedId = this->activeSessionId;
                            break;
                        }
                    }
                }
                // Fallback to first session
                if (selectedId == 0) {
                    selectedId = sessions[0].at("id").get<uint64_t>();
                }
                this->activeSessionId = selectedId;
                if (this->clientStorage) {
                    this->clientStorage->setUInt64(KEY_LAST_SESSION_ID, selectedId);
                }
                fmt::print("ClientCoreController: Selected session {}\n", selectedId);
                
                // Request history for selected session
                this->webSocketController->send(serialize(GetHistoryMessage{selectedId}));
            }
        } else if (messageType == "session_created") {
            // Handle session_created - auto-switch to new session
            uint64_t sessionId = serverData.at("session_id").get<uint64_t>();
            this->activeSessionId = sessionId;
            if (this->clientStorage) {
                this->clientStorage->setUInt64(KEY_LAST_SESSION_ID, sessionId);
            }
            fmt::print("ClientCoreController: Session created and activated: {}\n", sessionId);
        } else if (messageType == "session_closed") {
            uint64_t sessionId = serverData.at("session_id").get<uint64_t>();
            if (this->activeSessionId == sessionId) {
                this->activeSessionId = 0;
                fmt::print("ClientCoreController: Active session {} closed, resetting to 0\n", sessionId);
            }
        } else if (messageType == "command_start") {
            if (auto cmd = this->lastSentCommand.take()) {
                serverData["command"] = std::move(*cmd);
            }
        } else if (messageType == "output") {
            std::string_view rawOutput = serverData.at("data").get<std::string_view>();
            auto segments = this->ansiParser->parse(rawOutput);
            serverData["segments"] = std::move(segments);
            serverData.erase("data");
        } else if (messageType == "history") {
            auto& commands = serverData.at("commands");
            for (auto& cmd : commands) {
                if (auto it = cmd.find("output"); it != cmd.end() && it->is_string()) {
                    std::string_view rawOutput = it->get<std::string_view>();
                    auto segments = this->ansiParser->parse(rawOutput);
                    cmd["segments"] = std::move(segments);
                    cmd.erase(it);
                }
            }
        }
        
        this->pushEvent(json{
            {"type", "serverMessage"},
            {"data", std::move(serverData)}
        }.dump());
    } catch (const std::exception& e) {
        fmt::print(stderr, "ClientCoreController: Failed to parse server message: {}\n", e.what());
    }
}

void ClientCoreController::onWebSocketClose() {
    fmt::print("ClientCoreController::onWebSocketClose [thread:{}]\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    
    this->activeSessionId = 0;
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "disconnected"}
    }.dump());
}

void ClientCoreController::onWebSocketError(const std::string& error) {
    fmt::print("ClientCoreController::onWebSocketError [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()),
               error);
    
    this->pushEvent(json{
        {"type", "error"},
        {"message", error}
    }.dump());
}

// Simple C++ API (uses static instance)

const char* getVersion() {
    return ClientCoreController::getVersion();
}

bool initialize() {
    return ClientCoreController::instance.initialize();
}

void shutdown() {
    ClientCoreController::instance.shutdown();
}

bool isInitialized() {
    return ClientCoreController::instance.isInitialized();
}

const char* sendMessage(const char* message) {
    ClientCoreController::instance.sendMessage(message ? message : "");
    return ClientCoreController::instance.getLastResponseCStr();
}

const char* pollEvent() {
    return ClientCoreController::instance.pollEvent();
}

int pendingEventsCount() {
    return static_cast<int>(ClientCoreController::instance.pendingEventsCount());
}

void update() {
    ClientCoreController::instance.update();
}

} // namespace termihui

// C API implementation (extern "C")

extern "C" {

const char* termihui_get_version(void) {
    return termihui::getVersion();
}

bool termihui_initialize(void) {
    return termihui::initialize();
}

void termihui_shutdown(void) {
    termihui::shutdown();
}

bool termihui_is_initialized(void) {
    return termihui::isInitialized();
}

const char* termihui_send_message(const char* message) {
    return termihui::sendMessage(message);
}

const char* termihui_poll_event(void) {
    return termihui::pollEvent();
}

int termihui_pending_events_count(void) {
    return termihui::pendingEventsCount();
}

void termihui_update(void) {
    termihui::update();
}

}
