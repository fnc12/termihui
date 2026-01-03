#include "termihui/websocket_client_controller_impl.h"

namespace termihui {

WebSocketClientControllerImpl::WebSocketClientControllerImpl()
    : client(std::make_unique<hv::WebSocketClient>()) {
    
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

WebSocketClientControllerImpl::~WebSocketClientControllerImpl() {
    this->close();
}

int WebSocketClientControllerImpl::open(std::string_view url) {
    return this->client->open(std::string(url).c_str());
}

void WebSocketClientControllerImpl::close() {
    if (this->client) {
        this->client->close();
    }
}

bool WebSocketClientControllerImpl::isConnected() const {
    return this->client && this->client->isConnected();
}

int WebSocketClientControllerImpl::send(const std::string& message) {
    if (!this->client) {
        return -1;
    }
    return this->client->send(message);
}

std::vector<WebSocketClientController::Event> WebSocketClientControllerImpl::update() {
    return this->eventQueue.takeAll();
}

} // namespace termihui
