#pragma once

#include "termihui/websocket_client_controller.h"
#include <vector>

/**
 * Mock WebSocketClientController for testing
 */
class MockWebSocketClientController final : public WebSocketClientController {
public:
    // Events to return from update()
    std::vector<Event> eventsToReturn;
    
    // Track calls
    int updateCallCount = 0;
    int openCallCount = 0;
    int closeCallCount = 0;
    int sendCallCount = 0;
    
    // Mock state
    bool connected = false;
    
    int open(std::string_view /*url*/) override {
        ++openCallCount;
        return 0;
    }
    
    void close() override {
        ++closeCallCount;
        connected = false;
    }
    
    bool isConnected() const override {
        return connected;
    }
    
    int send(const std::string& /*message*/) override {
        ++sendCallCount;
        return 0;
    }
    
    std::vector<Event> update() override {
        ++updateCallCount;
        auto events = std::move(eventsToReturn);
        eventsToReturn.clear();
        return events;
    }
};
