#pragma once

#include <string>
#include <string_view>
#include <memory>
#include "thread_safe_queue.h"
#include "thread_safe_string.h"

// Forward declare libhv types
namespace hv {
    class WebSocketClient;
}

namespace termihui {

// Forward declare
class ANSIParser;

/**
 * TermiHUI Client Core Controller
 * Manages connection state and protocol handling
 */
class ClientCoreController {
public:
    static ClientCoreController instance;
    
    ClientCoreController();
    virtual ~ClientCoreController();
    
    // Disable copying and moving
    ClientCoreController(const ClientCoreController&) = delete;
    ClientCoreController& operator=(const ClientCoreController&) = delete;
    ClientCoreController(ClientCoreController&&) = delete;
    ClientCoreController& operator=(ClientCoreController&&) = delete;
    
    /**
     * Get client core version
     * @return version string
     */
    static const char* getVersion();
    
    /**
     * Initialize client core
     * @return true if successful
     */
    bool initialize();
    
    /**
     * Shutdown client core
     */
    void shutdown();
    
    /**
     * Check if initialized
     */
    bool isInitialized() const { return initialized; }
    
    /**
     * Send message to core for processing
     * @param message JSON or other encoded message
     * @return response string (JSON)
     */
    std::string sendMessage(std::string_view message);
    
    /**
     * Poll single event from queue
     * @return event string or nullptr if empty
     */
    const char* pollEvent();
    
    /**
     * Get count of pending events (thread-safe)
     */
    size_t pendingEventsCount() const;
    
    /**
     * Add event to pending queue (for internal use)
     * @param event JSON encoded event
     */
    void pushEvent(std::string event);
    
    /**
     * Get last response (for C API)
     */
    const char* getLastResponseCStr() const { return lastResponse.c_str(); }

protected:
    // Message handlers (virtual for testing)
    virtual void handleConnectButtonClicked(std::string_view address);
    virtual void handleDisconnectButtonClicked();
    virtual void handleExecuteCommand(std::string_view command);
    virtual void handleSendInput(std::string_view text);
    virtual void handleResize(int cols, int rows);
    virtual void handleRequestCompletion(std::string_view text, int cursorPosition);
    virtual void handleRequestReconnect(std::string_view address);

private:
    
    // WebSocket callbacks (called from libhv thread)
    void onWebSocketOpen();
    void onWebSocketMessage(const std::string& message);
    void onWebSocketClose();
    void onWebSocketError(const std::string& error);
    
    bool initialized = false;
    
    // WebSocket client
    std::unique_ptr<hv::WebSocketClient> wsClient;
    std::string serverAddress;
    ThreadSafeString lastSentCommand;
    
    // Event queue (thread-safe)
    StringQueue pendingEvents;
    
    // Buffers for C API (to return stable pointers)
    std::string lastResponse;
    std::string lastEvent;
    
    // ANSI parser for terminal output
    std::unique_ptr<ANSIParser> ansiParser;
};

// Simple C++ API (uses global instance)

/**
 * Get library version
 * @return version string (static, do not free)
 */
const char* getVersion();

/**
 * Initialize client core
 * @return true if successful
 */
bool initialize();

/**
 * Shutdown client core
 */
void shutdown();

/**
 * Check if client core is initialized
 * @return true if initialized
 */
bool isInitialized();

/**
 * Send message to client core
 * @param message JSON or other encoded message
 * @return response string (valid until next call, do not free)
 */
const char* sendMessage(const char* message);

/**
 * Poll single event from client core
 * Call in loop until nullptr returned
 * @return event JSON string or nullptr if empty (valid until next call)
 */
const char* pollEvent();

/**
 * Get count of pending events
 */
int pendingEventsCount();

} // namespace termihui
