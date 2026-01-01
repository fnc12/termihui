#pragma once

#include "TerminalSessionController.h"
#include "WebSocketServer.h"
#include "FileSystemManager.h"
#include "ServerStorage.h"
#include "CompletionManager.h"
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>

/**
 * Main TermiHUI server class
 * Manages terminal session, WebSocket connections, and command history
 */
class TermihuiServerController {
public:
    /**
     * Constructor
     * @param webSocketServer WebSocket server instance (dependency injection)
     */
    TermihuiServerController(std::unique_ptr<WebSocketServer> webSocketServer);
    
    /**
     * Destructor (virtual for testability)
     */
    virtual ~TermihuiServerController();
    
    // Disable copying
    TermihuiServerController(const TermihuiServerController&) = delete;
    TermihuiServerController& operator=(const TermihuiServerController&) = delete;
    
    /**
     * Start the server
     * @return true if started successfully
     */
    bool start();
    
    /**
     * Stop the server
     */
    void stop();
    
    /**
     * Main update loop iteration
     * Should be called repeatedly from main loop
     */
    void update();
    
    /**
     * Check if server should exit
     */
    bool shouldStop() const { return shouldExit.load(); }
    
    /**
     * Signal handler for graceful shutdown
     */
    static void signalHandler(int signal);
    
    /**
     * Handle incoming message from client (dispatcher)
     */
    void handleMessage(const WebSocketServer::IncomingMessage& message);

    /**
     * Process terminal output and OSC markers
     * @param session terminal session to process output from
     */
    void processTerminalOutput(TerminalSessionController& session);

protected:
    // Message handlers (virtual for testability)
    virtual void handleExecuteMessage(int clientId, uint64_t sessionId, const std::string& command);
    virtual void handleInputMessage(int clientId, uint64_t sessionId, const std::string& text);
    virtual void handleCompletionMessage(int clientId, uint64_t sessionId, const std::string& text, int cursorPosition);
    virtual void handleResizeMessage(int clientId, uint64_t sessionId, int cols, int rows);
    virtual void handleListSessionsMessage(int clientId);
    virtual void handleCreateSessionMessage(int clientId);
    virtual void handleCloseSessionMessage(int clientId, uint64_t sessionId);
    virtual void handleGetHistoryMessage(int clientId, uint64_t sessionId);
    
    /**
     * Get session by ID, returns nullptr if not found
     */
    TerminalSessionController* findSession(uint64_t sessionId);

private:
    /**
     * Handle new client connection
     */
    void handleNewConnection(int clientId);
    
    /**
     * Handle client disconnection
     */
    void handleDisconnection(int clientId);
    
    /**
     * Print server statistics
     */
    void printStats();
    
private:
    // Static flag for signal handling
    static std::atomic<bool> shouldExit;
    
    // Server components
    FileSystemManager fileSystemManager;
    std::unique_ptr<ServerStorage> serverStorage;
    std::unique_ptr<WebSocketServer> webSocketServer;
    CompletionManager completionManager;
    
    // Terminal sessions (sessionId -> controller)
    std::unordered_map<uint64_t, std::unique_ptr<TerminalSessionController>> sessions;
    
    // State tracking
    uint64_t currentRunId = 0;
    std::chrono::steady_clock::time_point lastStatsTime;
};
