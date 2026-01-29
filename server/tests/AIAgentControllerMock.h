#pragma once

#include "AIAgentController.h"
#include <variant>
#include <vector>
#include <string>
#include <ostream>

/**
 * Mock AI Agent Controller for unit tests
 * Records all calls and allows configuring return values
 */
class AIAgentControllerMock : public AIAgentController {
public:
    // Call record structures
    struct SetEndpointCall {
        std::string endpoint;
        bool operator==(const SetEndpointCall& other) const = default;
        friend std::ostream& operator<<(std::ostream& os, const SetEndpointCall& call) {
            return os << "SetEndpointCall{endpoint=" << call.endpoint << "}";
        }
    };
    
    struct SetModelCall {
        std::string model;
        bool operator==(const SetModelCall& other) const = default;
        friend std::ostream& operator<<(std::ostream& os, const SetModelCall& call) {
            return os << "SetModelCall{model=" << call.model << "}";
        }
    };
    
    struct SetApiKeyCall {
        std::string apiKey;
        bool operator==(const SetApiKeyCall& other) const = default;
        friend std::ostream& operator<<(std::ostream& os, const SetApiKeyCall& call) {
            return os << "SetApiKeyCall{apiKey=" << call.apiKey << "}";
        }
    };
    
    struct SendMessageCall {
        uint64_t sessionId = 0;
        std::string message;
        bool operator==(const SendMessageCall& other) const = default;
        friend std::ostream& operator<<(std::ostream& os, const SendMessageCall& call) {
            return os << "SendMessageCall{sessionId=" << call.sessionId << ", message=" << call.message << "}";
        }
    };
    
    struct UpdateCall {
        bool operator==(const UpdateCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const UpdateCall&) {
            return os << "UpdateCall{}";
        }
    };
    
    struct ClearHistoryCall {
        uint64_t sessionId = 0;
        bool operator==(const ClearHistoryCall& other) const = default;
        friend std::ostream& operator<<(std::ostream& os, const ClearHistoryCall& call) {
            return os << "ClearHistoryCall{sessionId=" << call.sessionId << "}";
        }
    };
    
    using Call = std::variant<SetEndpointCall, SetModelCall, SetApiKeyCall, SendMessageCall, UpdateCall, ClearHistoryCall>;
    
    friend std::ostream& operator<<(std::ostream& os, const Call& call) {
        std::visit([&os](const auto& c) { os << c; }, call);
        return os;
    }
    
    // Call recording
    std::vector<Call> calls;
    
    // Configurable return value for update()
    std::vector<AIEvent> updateReturnValue;
    
    // Interface implementation
    void setEndpoint(std::string endpoint) override {
        this->calls.push_back(SetEndpointCall{std::move(endpoint)});
    }
    
    void setModel(std::string model) override {
        this->calls.push_back(SetModelCall{std::move(model)});
    }
    
    void setApiKey(std::string apiKey) override {
        this->calls.push_back(SetApiKeyCall{std::move(apiKey)});
    }
    
    void sendMessage(uint64_t sessionId, const std::string& message) override {
        this->calls.push_back(SendMessageCall{sessionId, message});
    }
    
    std::vector<AIEvent> update() override {
        this->calls.push_back(UpdateCall{});
        return this->updateReturnValue;
    }
    
    void clearHistory(uint64_t sessionId) override {
        this->calls.push_back(ClearHistoryCall{sessionId});
    }
};
