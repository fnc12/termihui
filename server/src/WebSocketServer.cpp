#include "WebSocketServer.h"
#include <iostream>
#include <fmt/core.h>

// libhv headers included via WebSocketServer.h

WebSocketServer::WebSocketServer(int port)
    : port(port)
{
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start()
{
    if (this->running.exchange(true)) {
        return false; // Already running
    }
    
    try {
        // Setup callbacks - they will be called in libhv background thread
        this->wsService.onopen = [this](const WebSocketChannelPtr& channel, [[maybe_unused]] const HttpRequestPtr& request) {
            this->onConnection(channel);
        };
        
        this->wsService.onmessage = [this](const WebSocketChannelPtr& channel, const std::string& msg) {
            this->onMessage(channel, msg);
        };
        
        this->wsService.onclose = [this](const WebSocketChannelPtr& channel) {
            this->onClose(channel);
        };
        
        // Setup server
        this->wsServer.ws = &this->wsService;
        this->wsServer.port = this->port;
        
        // Start server in separate thread
        this->serverThread = std::make_unique<std::thread>([this]() {
            fmt::print("Starting WebSocket server on port {}\n", this->port);
            this->wsServer.run(false); // false = don't block thread
        });
        
        fmt::print("WebSocket server started on port {}\n", this->port);
        return true;
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "WebSocket server start error: {}\n", e.what());
        this->running = false;
        return false;
    }
}

void WebSocketServer::stop()
{
    if (!this->running.exchange(false)) {
        return; // Already stopped
    }
    
    // Close all connections
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        for (auto& [clientId, channel] : this->clients) {
            try {
                channel->close();
            } catch (const std::exception& e) {
                fmt::print(stderr, "Connection {} close error: {}\n", clientId, e.what());
            }
        }
        this->clients.clear();
        this->channelToClientId.clear();
    }
    
    // Stop server
    this->wsServer.stop();
    
    // Wait for server thread to finish
    if (this->serverThread && this->serverThread->joinable()) {
        this->serverThread->join();
    }
    
    // Clear all queues
    {
        std::lock_guard<std::mutex> lock(this->incomingMutex);
        std::queue<IncomingMessage> empty;
        this->incomingQueue.swap(empty);
    }
    
    {
        std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
        std::queue<ConnectionEvent> empty;
        this->connectionEventsQueue.swap(empty);
    }
    
    {
        std::lock_guard<std::mutex> lock(this->outgoingMutex);
        std::queue<OutgoingMessage> empty;
        this->outgoingQueue.swap(empty);
    }
    
    fmt::print("WebSocket server stopped\n");
}

bool WebSocketServer::isRunning() const
{
    return this->running.load();
}

void WebSocketServer::update(std::vector<IncomingMessage>& incomingMessages,
                           std::vector<ConnectionEvent>& connectionEvents)
{
    // Get all incoming messages
    {
        std::lock_guard<std::mutex> lock(this->incomingMutex);
        while (!this->incomingQueue.empty()) {
            incomingMessages.push_back(std::move(this->incomingQueue.front()));
            this->incomingQueue.pop();
        }
    }
    
    // Get all connection events
    {
        std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
        while (!this->connectionEventsQueue.empty()) {
            connectionEvents.push_back(std::move(this->connectionEventsQueue.front()));
            this->connectionEventsQueue.pop();
        }
    }
    
    // Process outgoing messages
    this->processOutgoingMessages();
}

void WebSocketServer::sendMessage(int clientId, const std::string& message)
{
    std::lock_guard<std::mutex> lock(this->outgoingMutex);
    this->outgoingQueue.push({clientId, message});
}

void WebSocketServer::broadcastMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(this->outgoingMutex);
    this->outgoingQueue.push({0, message}); // 0 = broadcast to all
}

size_t WebSocketServer::getConnectedClients() const
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    return this->clients.size();
}

std::vector<int> WebSocketServer::getClientIds() const
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    
    std::vector<int> result;
    result.reserve(this->clients.size());
    
    for (const auto& [clientId, channel] : this->clients) {
        result.push_back(clientId);
    }
    
    return result;
}

int WebSocketServer::generateClientId()
{
    return this->nextClientId.fetch_add(1);
}

void WebSocketServer::onConnection(const WebSocketChannelPtr& channel)
{
    // Generate ID for new client
    int clientId = this->generateClientId();
    
    // Save connection
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        this->clients[clientId] = channel;
        this->channelToClientId[channel] = clientId;
    }
    
    fmt::print("WebSocket connection: {} (address: {})\n", clientId, channel->peeraddr());
    
    // Add event to queue for processing in main thread
    {
        std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
        this->connectionEventsQueue.push({clientId, true});
    }
}

void WebSocketServer::onMessage(const WebSocketChannelPtr& channel, const std::string& message)
{
    // Get client ID
    int clientId;
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        auto it = this->channelToClientId.find(channel);
        if (it == this->channelToClientId.end()) {
            fmt::print(stderr, "Received message from unknown client\n");
            return;
        }
        clientId = it->second;
    }
    
    fmt::print("Received message from {}: {}\n", clientId, message);
    
    // Add message to queue for processing in main thread
    {
        std::lock_guard<std::mutex> lock(this->incomingMutex);
        this->incomingQueue.push({clientId, message});
    }
}

void WebSocketServer::onClose(const WebSocketChannelPtr& channel)
{
    // Get client ID
    int clientId = 0;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        auto it = this->channelToClientId.find(channel);
        if (it != this->channelToClientId.end()) {
            clientId = it->second;
            found = true;
            this->channelToClientId.erase(it);
            this->clients.erase(clientId);
        }
    }
    
    if (found) {
        fmt::print("WebSocket disconnect: {}\n", clientId);
        
        // Add event to queue for processing in main thread
        {
            std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
            this->connectionEventsQueue.push({clientId, false});
        }
    }
}

void WebSocketServer::processOutgoingMessages()
{
    std::vector<OutgoingMessage> messages;
    
    // Get all outgoing messages
    {
        std::lock_guard<std::mutex> lock(this->outgoingMutex);
        while (!this->outgoingQueue.empty()) {
            messages.push_back(std::move(this->outgoingQueue.front()));
            this->outgoingQueue.pop();
        }
    }
    
    // Send messages
    std::lock_guard<std::mutex> clientsLock(this->clientsMutex);
    for (const auto& msg : messages) {
        if (msg.clientId == 0) {
            // Broadcast to all clients
            for (const auto& [clientId, channel] : this->clients) {
                try {
                    channel->send(msg.message);
                } catch (const std::exception& e) {
                    fmt::print(stderr, "Broadcast message send error to client {}: {}\n", clientId, e.what());
                }
            }
        } else {
            // Send to specific client
            auto it = this->clients.find(msg.clientId);
            if (it != this->clients.end()) {
                try {
                    it->second->send(msg.message);
                } catch (const std::exception& e) {
                    fmt::print(stderr, "Message send error to client {}: {}\n", msg.clientId, e.what());
                }
            } else {
                fmt::print(stderr, "Client {} not found to send message\n", msg.clientId);
            }
        }
    }
}