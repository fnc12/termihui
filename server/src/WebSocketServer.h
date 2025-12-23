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

/**
 * WebSocket server implementation using libhv
 */
class WebSocketServerImpl : public WebSocketServer {
public:
    /**
     * Constructor
     * @param port port for WebSocket server
     * @param bindAddress address to bind (e.g. "0.0.0.0" or "127.0.0.1")
     */
    WebSocketServerImpl(int port, std::string bindAddress);
    
    /**
     * Destructor
     */
    ~WebSocketServerImpl() override;
    
    // Disable copying
    WebSocketServerImpl(const WebSocketServerImpl&) = delete;
    WebSocketServerImpl& operator=(const WebSocketServerImpl&) = delete;
    
    bool start() override;
    void stop() override;
    bool isRunning() const override;
    UpdateResult update() override;
    void sendMessage(int clientId, const std::string& message) override;
    void broadcastMessage(const std::string& message) override;
    size_t getConnectedClients() const override;
    int getPort() const override { return this->port; }
    const std::string& getBindAddress() const override { return this->bindAddress; }

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
    int port = 0;
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
};
