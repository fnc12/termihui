#include "termihui/client_core.h"
#include "termihui/client_core_c.h"
#include <fmt/core.h>
#include <thread>
#include <hv/WebSocketClient.h>
#include <hv/json.hpp>

using json = nlohmann::json;

namespace termihui {

// Version
static constexpr const char* VERSION = "1.0.0";

// ClientCore singleton implementation

ClientCore& ClientCore::instance() {
    static ClientCore instance;
    return instance;
}

ClientCore::ClientCore() {
    this->wsClient = std::make_unique<hv::WebSocketClient>();
}

ClientCore::~ClientCore() {
    if (this->initialized) {
        this->shutdown();
    }
}

const char* ClientCore::getVersion() {
    return VERSION;
}

bool ClientCore::initialize() {
    if (this->initialized) {
        fmt::print(stderr, "ClientCore: Already initialized\n");
        return false;
    }
    
    fmt::print("ClientCore: Initializing v{}\n", VERSION);
    this->initialized = true;
    fmt::print("ClientCore: Initialized successfully\n");
    
    return true;
}

void ClientCore::shutdown() {
    if (!this->initialized) {
        return;
    }
    
    fmt::print("ClientCore: Shutting down\n");
    
    // Close WebSocket connection
    if (this->wsClient) {
        this->wsClient->close();
    }
    
    this->pendingEvents.clear();
    this->lastResponse.clear();
    this->lastEvent.clear();
    this->serverAddress.clear();
    this->initialized = false;
}

std::string ClientCore::sendMessage(std::string_view message) {
    if (!this->initialized) {
        return "";
    }
    
    // DEBUG: Print thread id
    fmt::print("ClientCore::sendMessage [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()), 
               message);
    
    // Parse JSON message
    try {
        auto j = json::parse(message);
        std::string type = j.at("type").get<std::string>();
        
        if (type == "connectButtonClicked") {
            this->handleConnectButtonClicked(j.at("address").get<std::string>());
        } else if (type == "requestReconnect") {
            this->handleRequestReconnect(j.at("address").get<std::string>());
        } else if (type == "disconnectButtonClicked") {
            this->handleDisconnectButtonClicked();
        } else if (type == "executeCommand") {
            this->handleExecuteCommand(j.at("command").get<std::string>());
        } else if (type == "sendInput") {
            this->handleSendInput(j.at("text").get<std::string>());
        } else if (type == "resize") {
            this->handleResize(j.at("cols").get<int>(), j.at("rows").get<int>());
        } else if (type == "requestCompletion") {
            this->handleRequestCompletion(j.at("text").get<std::string>(), j.at("cursorPosition").get<int>());
        } else {
            fmt::print("ClientCore: Unknown message type: {}\n", type);
        }
    } catch (const std::exception& e) {
        fmt::print(stderr, "ClientCore: Failed to parse message: {}\n", e.what());
    }
    
    return "";
}

const char* ClientCore::pollEvent() {
    auto event = this->pendingEvents.pop();
    if (!event.has_value()) {
        return nullptr;
    }
    
    // Store in buffer for C API stability
    this->lastEvent = std::move(event.value());
    return this->lastEvent.c_str();
}

void ClientCore::pushEvent(std::string event) {
    // DEBUG: Print thread id
    fmt::print("ClientCore::pushEvent [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()), 
               event);
    
    this->pendingEvents.push(std::move(event));
}

size_t ClientCore::pendingEventsCount() const {
    return this->pendingEvents.size();
}

// Message handlers

void ClientCore::handleConnectButtonClicked(const std::string& address) {
    fmt::print("ClientCore: Connect button clicked, address: {}\n", address);
    
    if (address.empty()) {
        this->pushEvent(json{{"type", "error"}, {"message", "Address is empty"}}.dump());
        return;
    }
    
    this->serverAddress = address;
    
    // Build WebSocket URL
    std::string wsUrl = "ws://" + address;
    
    // Notify UI that we're connecting
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "connecting"},
        {"address", address}
    }.dump());
    
    // Setup WebSocket callbacks
    this->wsClient->onopen = [this]() {
        this->onWebSocketOpen();
    };
    
    this->wsClient->onmessage = [this](const std::string& msg) {
        this->onWebSocketMessage(msg);
    };
    
    this->wsClient->onclose = [this]() {
        this->onWebSocketClose();
    };
    
    // Connect
    fmt::print("ClientCore: Connecting to {}\n", wsUrl);
    int ret = this->wsClient->open(wsUrl.c_str());
    if (ret != 0) {
        this->onWebSocketError("Failed to initiate connection");
    }
}

void ClientCore::handleRequestReconnect(const std::string& address) {
    fmt::print("ClientCore: Auto-reconnect requested, address: {}\n", address);
    
    if (address.empty()) {
        this->pushEvent(json{{"type", "error"}, {"message", "Address is empty"}}.dump());
        return;
    }
    
    this->serverAddress = address;
    std::string wsUrl = "ws://" + address;
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "connecting"},
        {"address", address}
    }.dump());
    
    this->wsClient->onopen = [this]() { this->onWebSocketOpen(); };
    this->wsClient->onmessage = [this](const std::string& msg) { this->onWebSocketMessage(msg); };
    this->wsClient->onclose = [this]() { this->onWebSocketClose(); };
    
    fmt::print("ClientCore: Reconnecting to {}\n", wsUrl);
    int ret = this->wsClient->open(wsUrl.c_str());
    if (ret != 0) {
        this->onWebSocketError("Failed to initiate reconnection");
    }
}

void ClientCore::handleDisconnectButtonClicked() {
    fmt::print("ClientCore: Disconnect button clicked\n");
    
    if (this->wsClient) {
        this->wsClient->close();
    }
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "disconnected"}
    }.dump());
}

void ClientCore::handleExecuteCommand(const std::string& command) {
    fmt::print("ClientCore: Execute command: {}\n", command);
    
    if (!this->wsClient || !this->wsClient->isConnected()) {
        this->pushEvent(json{
            {"type", "error"},
            {"message", "Not connected to server"}
        }.dump());
        return;
    }
    
    // Save for block header
    this->lastSentCommand = command;
    
    json msg = {
        {"type", "execute"},
        {"command", command}
    };
    this->wsClient->send(msg.dump());
}

void ClientCore::handleSendInput(const std::string& text) {
    fmt::print("ClientCore: Send input: {}\n", text.substr(0, 20));
    
    if (!this->wsClient || !this->wsClient->isConnected()) {
        return;
    }
    
    json msg = {
        {"type", "input"},
        {"text", text}
    };
    this->wsClient->send(msg.dump());
}

void ClientCore::handleResize(int cols, int rows) {
    fmt::print("ClientCore: Resize: {}x{}\n", cols, rows);
    
    if (!this->wsClient || !this->wsClient->isConnected()) {
        return;
    }
    
    json msg = {
        {"type", "resize"},
        {"cols", cols},
        {"rows", rows}
    };
    this->wsClient->send(msg.dump());
}

void ClientCore::handleRequestCompletion(const std::string& text, int cursorPosition) {
    fmt::print("ClientCore: Request completion for: '{}' at {}\n", text, cursorPosition);
    
    if (!this->wsClient || !this->wsClient->isConnected()) {
        return;
    }
    
    json msg = {
        {"type", "completion"},
        {"text", text},
        {"cursor_position", cursorPosition}
    };
    this->wsClient->send(msg.dump());
}

// WebSocket callbacks (called from libhv thread)

void ClientCore::onWebSocketOpen() {
    fmt::print("ClientCore::onWebSocketOpen [thread:{}]\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "connected"},
        {"address", this->serverAddress}
    }.dump());
}

void ClientCore::onWebSocketMessage(const std::string& message) {
    fmt::print("ClientCore::onWebSocketMessage [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()),
               message.substr(0, 100)); // Truncate for debug
    
    // Forward message to UI as event
    try {
        auto serverData = json::parse(message);
        
        // Add saved command to command_start
        if (serverData.value("type", "") == "command_start" && !this->lastSentCommand.empty()) {
            serverData["command"] = this->lastSentCommand;
            this->lastSentCommand.clear();
        }
        
        this->pushEvent(json{
            {"type", "serverMessage"},
            {"data", serverData}
        }.dump());
    } catch (const std::exception& e) {
        fmt::print(stderr, "ClientCore: Failed to parse server message: {}\n", e.what());
    }
}

void ClientCore::onWebSocketClose() {
    fmt::print("ClientCore::onWebSocketClose [thread:{}]\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "disconnected"}
    }.dump());
}

void ClientCore::onWebSocketError(const std::string& error) {
    fmt::print("ClientCore::onWebSocketError [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()),
               error);
    
    this->pushEvent(json{
        {"type", "error"},
        {"message", error}
    }.dump());
}

// Simple C++ API (uses singleton)

const char* getVersion() {
    return ClientCore::getVersion();
}

bool initialize() {
    return ClientCore::instance().initialize();
}

void shutdown() {
    ClientCore::instance().shutdown();
}

bool isInitialized() {
    return ClientCore::instance().isInitialized();
}

const char* sendMessage(const char* message) {
    auto& core = ClientCore::instance();
    core.sendMessage(message ? message : "");
    return core.getLastResponseCStr();
}

const char* pollEvent() {
    return ClientCore::instance().pollEvent();
}

int pendingEventsCount() {
    return static_cast<int>(ClientCore::instance().pendingEventsCount());
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

}
