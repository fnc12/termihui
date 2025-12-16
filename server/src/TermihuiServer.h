#pragma once

#include "TerminalSession.h"
#include "WebSocketServer.h"
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <chrono>

/**
 * Main TermiHUI server class
 * Manages terminal session, WebSocket connections, and command history
 */
class TermihuiServer {
public:
    /**
     * Constructor
     * @param port WebSocket server port
     * @param bindAddress address to bind (e.g. "0.0.0.0" or "127.0.0.1")
     */
    TermihuiServer(int port, std::string bindAddress);
    
    /**
     * Destructor
     */
    ~TermihuiServer();
    
    // Disable copying
    TermihuiServer(const TermihuiServer&) = delete;
    TermihuiServer& operator=(const TermihuiServer&) = delete;
    
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
     * Handle incoming message from client
     */
    void handleMessage(const WebSocketServer::IncomingMessage& msg);
    
    /**
     * Process terminal output and OSC markers
     */
    void processTerminalOutput();
    
    /**
     * Print server statistics
     */
    void printStats();
    
    /**
     * Structure for storing command history
     */
    struct CommandRecord {
        std::string command;
        std::string output;
        int exitCode = 0;
        std::string cwdStart;
        std::string cwdEnd;
        bool isFinished = false;
    };
    
private:
    // Static flag for signal handling
    static std::atomic<bool> shouldExit;
    
    // Server components
    WebSocketServer webSocketServer;
    std::unique_ptr<TerminalSession> terminalSession;
    
    // Command history
    std::vector<CommandRecord> commandHistory;
    int currentCommandIndex = -1;
    std::string pendingCommand;
    
    // State tracking
    bool wasRunning = false;
    std::chrono::steady_clock::time_point lastStatsTime;
};

