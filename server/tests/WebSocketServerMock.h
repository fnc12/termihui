#pragma once

#include "WebSocketServer.h"
#include "hv/json.hpp"
#include <variant>
#include <vector>
#include <string>

/**
 * Mock WebSocket server for unit tests
 * Records all calls to sendMessage and broadcastMessage
 * Stores messages as parsed JSON for easy comparison
 */
class WebSocketServerMock : public WebSocketServer {
public:
    using json = nlohmann::json;
    
    struct SendMessageCall {
        int clientId = 0;
        std::string message;
        bool operator==(const SendMessageCall& other) const {
            return this->clientId == other.clientId && 
                   json::parse(this->message) == json::parse(other.message);
        }
    };

    struct BroadcastMessageCall {
        std::string message;
        bool operator==(const BroadcastMessageCall& other) const {
            return json::parse(this->message) == json::parse(other.message);
        }
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

