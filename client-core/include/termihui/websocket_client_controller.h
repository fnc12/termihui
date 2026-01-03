#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

/**
 * Interface for WebSocket client controller.
 * Synchronizes WebSocket callbacks to main thread via update().
 */
class WebSocketClientController {
public:
    // Event types
    struct OpenEvent {};
    struct MessageEvent { std::string message; };
    struct CloseEvent {};
    struct ErrorEvent { std::string error; };
    
    using Event = std::variant<OpenEvent, MessageEvent, CloseEvent, ErrorEvent>;
    
    virtual ~WebSocketClientController() = default;
    
    /**
     * Open WebSocket connection to URL
     * @param url WebSocket URL (e.g. "ws://localhost:8080")
     * @return 0 on success, non-zero on error
     */
    virtual int open(std::string_view url) = 0;
    
    /**
     * Close WebSocket connection
     */
    virtual void close() = 0;
    
    /**
     * Check if connected
     */
    virtual bool isConnected() const = 0;
    
    /**
     * Send message to server
     * @param message Message to send
     * @return bytes sent, or negative on error
     */
    virtual int send(const std::string& message) = 0;
    
    /**
     * Get pending events and clear the queue.
     * Call this from main thread to process WebSocket events.
     * @return Vector of events that occurred since last update()
     */
    virtual std::vector<Event> update() = 0;
};
