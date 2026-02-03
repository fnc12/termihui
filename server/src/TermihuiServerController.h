#pragma once

#include "TerminalSessionController.h"
#include "WebSocketServer.h"
#include <termihui/filesystem/file_system_manager.h>
#include "ServerStorage.h"
#include "CompletionManager.h"
#include "AIAgentController.h"
#include "OutputParser.h"
#include <termihui/protocol/protocol.h>
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
     * @param aiAgentController AI agent controller instance (dependency injection)
     * @param serverStorage Server storage instance (dependency injection, optional - created in start() if null)
     */
    TermihuiServerController(std::unique_ptr<WebSocketServer> webSocketServer,
                             std::unique_ptr<AIAgentController> aiAgentController,
                             std::unique_ptr<ServerStorage> serverStorage);
    
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
    
    /**
     * Send full screen snapshot to clients
     * @param session terminal session
     */
    void sendScreenSnapshot(TerminalSessionController& session);
    
    /**
     * Send screen diff (changed rows only) to clients
     * @param session terminal session
     */
    void sendScreenDiff(TerminalSessionController& session);
    
    /**
     * Process output in block mode (OSC markers for command tracking)
     * @param session terminal session
     * @param output raw output string
     */
    void processBlockModeOutput(TerminalSessionController& session, const std::string& output, bool skipOutputRecording);

    /**
     * Shorten path by replacing home directory with ~
     * @param path full path
     * @return path with home replaced by ~ if applicable
     */
    std::string shortenHomePath(const std::string& path) const;

protected:
    // Type-safe message handlers (virtual for testability)
    virtual void handleMessageFromClient(int clientId, const ExecuteMessage& message);
    virtual void handleMessageFromClient(int clientId, const InputMessage& message);
    virtual void handleMessageFromClient(int clientId, const CompletionMessage& message);
    virtual void handleMessageFromClient(int clientId, const ResizeMessage& message);
    virtual void handleMessageFromClient(int clientId, const ListSessionsMessage& message);
    virtual void handleMessageFromClient(int clientId, const CreateSessionMessage& message);
    virtual void handleMessageFromClient(int clientId, const CloseSessionMessage& message);
    virtual void handleMessageFromClient(int clientId, const GetHistoryMessage& message);
    virtual void handleMessageFromClient(int clientId, const AIChatMessage& message);
    virtual void handleMessageFromClient(int clientId, const GetChatHistoryMessage& message);
    virtual void handleMessageFromClient(int clientId, const ListLLMProvidersMessage& message);
    virtual void handleMessageFromClient(int clientId, const AddLLMProviderMessage& message);
    virtual void handleMessageFromClient(int clientId, const UpdateLLMProviderMessage& message);
    virtual void handleMessageFromClient(int clientId, const DeleteLLMProviderMessage& message);
    
    /**
     * Get session by ID, returns nullptr if not found
     */
    TerminalSessionController* findSession(uint64_t sessionId);
    
    // Home directory for path shortening (cached at start)
    std::string homeDirectory;

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
    termihui::FileSystemManager fileSystemManager;
    std::unique_ptr<ServerStorage> serverStorage;
    std::unique_ptr<WebSocketServer> webSocketServer;
    std::unique_ptr<AIAgentController> aiAgentController;
    CompletionManager completionManager;
    termihui::OutputParser outputParser;
    
    // Terminal sessions (sessionId -> controller)
    std::unordered_map<uint64_t, std::unique_ptr<TerminalSessionController>> sessions;
    
    // UTF-8 pending buffers per session (for incomplete sequences between reads)
    std::unordered_map<uint64_t, std::string> utf8PendingBuffers;
    
    // State tracking
    uint64_t currentRunId = 0;
    std::chrono::steady_clock::time_point lastStatsTime;
};
