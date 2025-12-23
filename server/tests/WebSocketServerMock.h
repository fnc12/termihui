#pragma once

#include "WebSocketServer.h"
#include <variant>
#include <vector>
#include <string>

/**
 * Mock WebSocket server for unit tests
 * Records all calls to sendMessage and broadcastMessage
 */
class WebSocketServerMock : public WebSocketServer {
public:
    // Call record structures with default comparison (C++20)
    struct SendMessageCall {
        int clientId = 0;
        std::string message;
        auto operator<=>(const SendMessageCall&) const = default;
    };

    struct BroadcastMessageCall {
        std::string message;
        auto operator<=>(const BroadcastMessageCall&) const = default;
    };

    using Call = std::variant<SendMessageCall, BroadcastMessageCall>;

    // Call recording
    std::vector<Call> calls;

    // Stub implementations
    bool start() override { return true; }
    void stop() override {}
    bool isRunning() const override { return false; }
    
    UpdateResult update() override { return {}; }
    
    void sendMessage(int clientId, const std::string& message) override {
        this->calls.push_back(SendMessageCall{clientId, message});
    }
    
    void broadcastMessage(const std::string& message) override {
        this->calls.push_back(BroadcastMessageCall{message});
    }
    
    size_t getConnectedClients() const override { return 0; }
    int getPort() const override { return 0; }
    const std::string& getBindAddress() const override { return this->bindAddress; }

private:
    std::string bindAddress = "127.0.0.1";
};

