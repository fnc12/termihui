#pragma once

#include <string>
#include <vector>

/**
 * WebSocket server interface for terminal sessions
 * 
 * Features:
 * - WebSocket connection handling
 * - JSON protocol according to docs/protocol.md
 * - Non-blocking architecture with message queues
 * - Processing in main thread via update()
 */
class WebSocketServer {
public:
    /**
     * Structure for incoming message from client
     */
    struct IncomingMessage {
        int clientId = 0;
        std::string text;
    };
    
    /**
     * Structure for outgoing message to client
     */
    struct OutgoingMessage {
        int clientId = 0;  // 0 = broadcast to all
        std::string message;
    };
    
    /**
     * Client connection/disconnection event
     */
    struct ConnectionEvent {
        int clientId = 0;
        bool connected = false;  // true = connected, false = disconnected
    };
    
    /**
     * Result of update() call
     */
    struct UpdateResult {
        std::vector<IncomingMessage> incomingMessages;
        std::vector<ConnectionEvent> connectionEvents;
    };
    
    /**
     * Virtual destructor
     */
    virtual ~WebSocketServer() = default;
    
    /**
     * Start server (in background thread)
     * @return true if server started successfully
     */
    virtual bool start() = 0;
    
    /**
     * Stop server
     */
    virtual void stop() = 0;
    
    /**
     * Check if server is running
     * @return true if server is running
     */
    virtual bool isRunning() const = 0;
    
    /**
     * Update - process all accumulated events (call from main thread)
     * @return struct with incoming messages and connection events
     */
    virtual UpdateResult update() = 0;
    
    /**
     * Send message to client (adds to queue)
     * @param clientId client identifier
     * @param message message to send
     */
    virtual void sendMessage(int clientId, const std::string& message) = 0;
    
    /**
     * Broadcast message (adds to queue)
     * @param message message to send to all clients
     */
    virtual void broadcastMessage(const std::string& message) = 0;
    
    /**
     * Get number of connected clients
     * @return number of active connections
     */
    virtual size_t getConnectedClients() const = 0;
    
    /**
     * Get server port
     * @return port number
     */
    virtual int getPort() const = 0;
    
    /**
     * Get bind address
     * @return bind address string
     */
    virtual const std::string& getBindAddress() const = 0;
};
