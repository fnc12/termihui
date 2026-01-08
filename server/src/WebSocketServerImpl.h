#pragma once

#include "WebSocketServer.h"
#include <termihui/thread_safe_queue.h>

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

// Include libhv headers
#include "hv/WebSocketServer.h"

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
    
    // Thread-safe message queues
    termihui::ThreadSafeQueue<IncomingMessage> incomingQueue;
    termihui::ThreadSafeQueue<ConnectionEvent> connectionEventsQueue;
    termihui::ThreadSafeQueue<OutgoingMessage> outgoingQueue;
};
