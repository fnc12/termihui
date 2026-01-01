#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include "thread_safe_queue.h"
#include "thread_safe_string.h"

// Forward declare (outside namespace)
class WebSocketClientController;
class ClientStorage;

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
     * Main update tick - processes WebSocket events on main thread.
     * Should be called regularly (e.g. 60fps) from main thread.
     * 
     * WARNING: Do not perform long-running operations in this method
     * as it blocks the main thread.
     */
    void update();
    
    /**
     * Get last response (for C API)
     */
    const char* getLastResponseCStr() const { return lastResponse.c_str(); }

    /**
     * Get active session ID
     * @return active session ID (0 if none)
     */
    uint64_t getActiveSessionId() const { return activeSessionId; }

protected:
    // Message handlers (virtual for testing)
    // Return empty string on success, error message on failure
    virtual std::string handleConnectButtonClicked(std::string_view address);
    virtual std::string handleDisconnectButtonClicked();
    virtual std::string handleExecuteCommand(std::string_view command);
    virtual std::string handleSendInput(std::string_view text);
    virtual std::string handleResize(int cols, int rows);
    virtual std::string handleRequestCompletion(std::string_view text, int cursorPosition);
    virtual std::string handleRequestReconnect(std::string_view address);
    
    // Session management handlers
    virtual std::string handleCreateSession();
    virtual std::string handleCloseSession(uint64_t sessionId);
    virtual std::string handleSwitchSession(uint64_t sessionId);
    virtual std::string handleListSessions();

private:
    
    // WebSocket callbacks (called from libhv thread)
    void onWebSocketOpen();
    void onWebSocketMessage(const std::string& message);
    void onWebSocketClose();
    void onWebSocketError(const std::string& error);
    
    bool initialized = false;
    
    // WebSocket client controller (handles thread synchronization)
    std::unique_ptr<WebSocketClientController> webSocketController;
    std::string serverAddress;
    ThreadSafeString lastSentCommand;
    
    // Event queue (thread-safe)
    StringQueue pendingEvents;
    
    // Buffers for C API (to return stable pointers)
    std::string lastResponse;
    std::string lastEvent;
    
    // ANSI parser for terminal output
    std::unique_ptr<ANSIParser> ansiParser;
    
    // Persistent storage for client settings
    std::unique_ptr<ClientStorage> clientStorage;
    
    // Active session ID (0 = no session selected)
    uint64_t activeSessionId = 0;
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

/**
 * Update tick - process WebSocket events on main thread
 * Call this regularly (e.g. 60fps) before pollEvent()
 */
void update();

} // namespace termihui
