#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <vector>

// Include libhv headers
#include "hv/WebSocketServer.h"

/**
 * WebSocket server for terminal sessions
 * 
 * Features:
 * - WebSocket connection handling via libhv
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
     * Constructor
     * @param port port for WebSocket server
     * @param bindAddress address to bind (e.g. "0.0.0.0" or "127.0.0.1")
     */
    WebSocketServer(int port, std::string bindAddress);
    
    /**
     * Destructor
     */
    ~WebSocketServer();
    
    // Disable copying
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    
    /**
     * Start server (in background thread)
     * @return true if server started successfully
     */
    bool start();
    
    /**
     * Stop server
     */
    void stop();
    
    /**
     * Check if server is running
     * @return true if server is running
     */
    bool isRunning() const;
    
    /**
     * Update - process all accumulated events (call from main thread)
     * @return struct with incoming messages and connection events
     */
    UpdateResult update();
    
    /**
     * Send message to client (adds to queue)
     * @param clientId client identifier
     * @param message message to send
     */
    void sendMessage(int clientId, const std::string& message);
    
    /**
     * Broadcast message (adds to queue)
     * @param message message to send to all clients
     */
    void broadcastMessage(const std::string& message);
    
    /**
     * Get number of connected clients
     * @return number of active connections
     */
    size_t getConnectedClients() const;
    
    /**
     * Get server port
     * @return port number
     */
    int getPort() const { return port; }
    
    /**
     * Get bind address
     * @return bind address string
     */
    const std::string& getBindAddress() const { return bindAddress; }

private:
    /**
     * Generate unique client ID
     * @return unique identifier
     */
    int generateClientId();
    
    /**
     * libhv callbacks (executed in background thread)
     */
    void onConnection(const WebSocketChannelPtr& channel);
    void onMessage(const WebSocketChannelPtr& channel, const std::string& message);
    void onClose(const WebSocketChannelPtr& channel);
    
    /**
     * Send outgoing messages from queue (called from update)
     */
    void processOutgoingMessages();

private:
    int port;
    std::string bindAddress;
    std::atomic<bool> running{false};
    std::atomic<int> nextClientId{1};
    
    // libhv components
    hv::WebSocketService wsService;
    hv::WebSocketServer wsServer;
    std::unique_ptr<std::thread> serverThread;
    
    // Client management (mutex protected)
    mutable std::mutex clientsMutex;
    std::unordered_map<int, WebSocketChannelPtr> clients;
    std::unordered_map<WebSocketChannelPtr, int> channelToClientId;
    
    // Message queues (mutex protected)
    std::mutex incomingMutex;
    std::queue<IncomingMessage> incomingQueue;
    
    std::mutex connectionEventsMutex;
    std::queue<ConnectionEvent> connectionEventsQueue;
    
    std::mutex outgoingMutex;
    std::queue<OutgoingMessage> outgoingQueue;
    
    // TODO: Add in future:
    // - SSL/TLS support
    // - Client authentication
    // - Connection limit
    // - Heartbeat for connection checks
    // - Message compression
    // - Connection logging
}; 
