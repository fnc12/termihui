#pragma once

#include "thread_safe_queue.h"
#include <hv/WebSocketClient.h>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/**
 * Wrapper around hv::WebSocketClient that synchronizes callbacks to main thread.
 * 
 * WebSocket callbacks are invoked on libhv's background thread.
 * This class queues events and returns them via update() for processing on main thread.
 */
class WebSocketClientController {
public:
    // Event types
    struct OpenEvent {};
    struct MessageEvent { std::string message; };
    struct CloseEvent {};
    struct ErrorEvent { std::string error; };
    
    using Event = std::variant<OpenEvent, MessageEvent, CloseEvent, ErrorEvent>;
    
    WebSocketClientController();
    ~WebSocketClientController();
    
    /**
     * Open WebSocket connection to URL
     * @param url WebSocket URL (e.g. "ws://localhost:8080")
     * @return 0 on success, non-zero on error
     */
    int open(std::string_view url);
    
    /**
     * Close WebSocket connection
     */
    void close();
    
    /**
     * Check if connected
     */
    bool isConnected() const;
    
    /**
     * Send message to server
     * @param message Message to send
     * @return bytes sent, or negative on error
     */
    int send(const std::string& message);
    
    /**
     * Get pending events and clear the queue.
     * Call this from main thread to process WebSocket events.
     * @return Vector of events that occurred since last update()
     */
    std::vector<Event> update();

private:
    std::unique_ptr<hv::WebSocketClient> client;
    termihui::ThreadSafeQueue<Event> eventQueue;
};
