#include "WebSocketServerImpl.h"
#include <iostream>
#include <cstring>
#include <fmt/core.h>

// libhv headers included via WebSocketServer.h

WebSocketServerImpl::WebSocketServerImpl(int port, std::string bindAddress)
    : port(port)
    , bindAddress(std::move(bindAddress))
{
}

WebSocketServerImpl::~WebSocketServerImpl()
{
    stop();
}

bool WebSocketServerImpl::start()
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
        strncpy(this->wsServer.host, this->bindAddress.c_str(), sizeof(this->wsServer.host) - 1);
        this->wsServer.host[sizeof(this->wsServer.host) - 1] = '\0';
        this->wsServer.port = this->port;
        
        // Start server in separate thread
        this->serverThread = std::make_unique<std::thread>([this]() {
            fmt::print("Starting WebSocket server on {}:{}\n", this->bindAddress, this->port);
            this->wsServer.run(false); // false = don't block thread
        });
        
        fmt::print("WebSocket server started on {}:{}\n", this->bindAddress, this->port);
        return true;
        
    } catch (const std::exception& e) {
        fmt::print(stderr, "WebSocket server start error: {}\n", e.what());
        this->running = false;
        return false;
    }
}

void WebSocketServerImpl::stop()
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
    this->incomingQueue.clear();
    this->connectionEventsQueue.clear();
    this->outgoingQueue.clear();
    
    fmt::print("WebSocket server stopped\n");
}

bool WebSocketServerImpl::isRunning() const
{
    return this->running.load();
}

WebSocketServerImpl::UpdateResult WebSocketServerImpl::update()
{
    UpdateResult result;
    
    // Get all incoming messages
    result.incomingMessages = this->incomingQueue.takeAll();
    
    // Get all connection events
    result.connectionEvents = this->connectionEventsQueue.takeAll();
    
    // Process outgoing messages
    this->processOutgoingMessages();
    
    return result;
}

void WebSocketServerImpl::sendMessage(int clientId, const std::string& message)
{
    this->outgoingQueue.push({clientId, message});
}

void WebSocketServerImpl::broadcastMessage(const std::string& message)
{
    this->outgoingQueue.push({0, message}); // 0 = broadcast to all
}

size_t WebSocketServerImpl::getConnectedClients() const
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    return this->clients.size();
}

int WebSocketServerImpl::generateClientId()
{
    return this->nextClientId.fetch_add(1);
}

void WebSocketServerImpl::onConnection(const WebSocketChannelPtr& channel)
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
    this->connectionEventsQueue.push({clientId, true});
}

void WebSocketServerImpl::onMessage(const WebSocketChannelPtr& channel, const std::string& message)
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
    this->incomingQueue.push({clientId, message});
}

void WebSocketServerImpl::onClose(const WebSocketChannelPtr& channel)
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
        this->connectionEventsQueue.push({clientId, false});
    }
}

void WebSocketServerImpl::processOutgoingMessages()
{
    // Get all outgoing messages
    auto messages = this->outgoingQueue.takeAll();
    
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
