#include "termihui/websocket_client_controller.h"

WebSocketClientController::WebSocketClientController()
    : client(std::make_unique<hv::WebSocketClient>()) {
    
    // Setup callbacks that queue events for main thread processing
    this->client->onopen = [this]() {
        this->eventQueue.push(OpenEvent{});
    };
    
    this->client->onmessage = [this](const std::string& msg) {
        this->eventQueue.push(MessageEvent{msg});
    };
    
    this->client->onclose = [this]() {
        this->eventQueue.push(CloseEvent{});
    };
}

WebSocketClientController::~WebSocketClientController() {
    this->close();
}

int WebSocketClientController::open(std::string_view url) {
    return this->client->open(std::string(url).c_str());
}

void WebSocketClientController::close() {
    if (this->client) {
        this->client->close();
    }
}

bool WebSocketClientController::isConnected() const {
    return this->client && this->client->isConnected();
}

int WebSocketClientController::send(const std::string& message) {
    if (!this->client) {
        return -1;
    }
    return this->client->send(message);
}

std::vector<WebSocketClientController::Event> WebSocketClientController::update() {
    return this->eventQueue.takeAll();
}
