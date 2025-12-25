#include "termihui/client_core.h"
#include "termihui/client_core_c.h"
#include "termihui/ansi_parser.h"
#include <fmt/core.h>
#include <thread>
#include <hv/WebSocketClient.h>
#include <hv/json.hpp>

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
    : wsClient(std::make_unique<hv::WebSocketClient>())
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
    if (this->wsClient) {
        this->wsClient->close();
    }
    
    this->pendingEvents.clear();
    this->lastResponse.clear();
    this->lastEvent.clear();
    this->serverAddress.clear();
    this->initialized = false;
}

std::string ClientCoreController::sendMessage(std::string_view message) {
    if (!this->initialized) {
        return "";
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
            fmt::print("ClientCoreController: Unknown message type: {}\n", type);
        }
    } catch (const std::exception& e) {
        fmt::print(stderr, "ClientCoreController: Failed to parse message: {}\n", e.what());
    }
    
    return "";
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

void ClientCoreController::handleConnectButtonClicked(const std::string& address) {
    fmt::print("ClientCoreController: Connect button clicked, address: {}\n", address);
    
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
    fmt::print("ClientCoreController: Connecting to {}\n", wsUrl);
    int ret = this->wsClient->open(wsUrl.c_str());
    if (ret != 0) {
        this->onWebSocketError("Failed to initiate connection");
    }
}

void ClientCoreController::handleRequestReconnect(const std::string& address) {
    fmt::print("ClientCoreController: Auto-reconnect requested, address: {}\n", address);
    
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
    
    fmt::print("ClientCoreController: Reconnecting to {}\n", wsUrl);
    int ret = this->wsClient->open(wsUrl.c_str());
    if (ret != 0) {
        this->onWebSocketError("Failed to initiate reconnection");
    }
}

void ClientCoreController::handleDisconnectButtonClicked() {
    fmt::print("ClientCoreController: Disconnect button clicked\n");
    
    if (this->wsClient) {
        this->wsClient->close();
    }
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "disconnected"}
    }.dump());
}

void ClientCoreController::handleExecuteCommand(const std::string& command) {
    fmt::print("ClientCoreController: Execute command: {}\n", command);
    
    if (!this->wsClient || !this->wsClient->isConnected()) {
        this->pushEvent(json{
            {"type", "error"},
            {"message", "Not connected to server"}
        }.dump());
        return;
    }
    
    // Save for block header
    this->lastSentCommand.set(command);
    
    json msg = {
        {"type", "execute"},
        {"command", command}
    };
    this->wsClient->send(msg.dump());
}

void ClientCoreController::handleSendInput(const std::string& text) {
    fmt::print("ClientCoreController: Send input: {}\n", text.substr(0, 20));
    
    if (!this->wsClient || !this->wsClient->isConnected()) {
        return;
    }
    
    json msg = {
        {"type", "input"},
        {"text", text}
    };
    this->wsClient->send(msg.dump());
}

void ClientCoreController::handleResize(int cols, int rows) {
    fmt::print("ClientCoreController: Resize: {}x{}\n", cols, rows);
    
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

void ClientCoreController::handleRequestCompletion(const std::string& text, int cursorPosition) {
    fmt::print("ClientCoreController: Request completion for: '{}' at {}\n", text, cursorPosition);
    
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

void ClientCoreController::onWebSocketOpen() {
    fmt::print("ClientCoreController::onWebSocketOpen [thread:{}]\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()));
    
    this->pushEvent(json{
        {"type", "connectionStateChanged"},
        {"state", "connected"},
        {"address", this->serverAddress}
    }.dump());
}

void ClientCoreController::onWebSocketMessage(const std::string& message) {
    fmt::print("ClientCoreController::onWebSocketMessage [thread:{}]: {}\n", 
               std::hash<std::thread::id>{}(std::this_thread::get_id()),
               message.substr(0, 100)); // Truncate for debug
    
    // Forward message to UI as event
    try {
        auto serverData = json::parse(message);
        std::string msgType = serverData.at("type").get<std::string>();
        
        // Add saved command to command_start
        if (msgType == "command_start") {
            if (auto cmd = this->lastSentCommand.take()) {
                serverData["command"] = std::move(*cmd);
            }
        }
        
        // Parse ANSI codes for output messages
        if (msgType == "output") {
            std::string rawOutput = serverData.at("data").get<std::string>();
            
            // Parse ANSI codes into styled segments (ADL to_json handles conversion)
            auto segments = this->ansiParser->parse(rawOutput);
            
            // Replace raw data with parsed segments
            serverData["segments"] = std::move(segments);
            serverData.erase("data");
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
