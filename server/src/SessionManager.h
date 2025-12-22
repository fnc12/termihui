#pragma once

#include "TerminalSessionController.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <chrono>

/**
 * Terminal session manager
 * 
 * Features:
 * - Managing multiple sessions (tabs)
 * - Thread-safe operations
 * - Non-blocking reading of all sessions
 * - Automatic cleanup of finished sessions
 */
class SessionManager {
public:
    using SessionId = std::string;
    using OutputCallback = std::function<void(const SessionId&, const std::string&)>;
    using StatusCallback = std::function<void(const SessionId&, bool isRunning)>;
    
    /**
     * Constructor
     * @param pollIntervalMs session polling interval in milliseconds
     */
    explicit SessionManager(int pollIntervalMs = 10);
    
    /**
     * Destructor
     */
    ~SessionManager();
    
    // Disable copying
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    
    /**
     * Start manager (begins background thread)
     */
    void start();
    
    /**
     * Stop manager
     */
    void stop();
    
    /**
     * Create new session
     * @param sessionId unique session identifier
     * @param command command to execute (default bash)
     * @return true if session created successfully
     */
    bool createSession(const SessionId& sessionId, const std::string& command = "bash");
    
    /**
     * Close session
     * @param sessionId session identifier
     * @return true if session was found and closed
     */
    bool closeSession(const SessionId& sessionId);
    
    /**
     * Send input to session
     * @param sessionId session identifier
     * @param input data to send
     * @return number of bytes sent, -1 on error
     */
    ssize_t sendInput(const SessionId& sessionId, const std::string& input);
    
    /**
     * Check if session exists
     * @param sessionId session identifier
     * @return true if session exists
     */
    bool hasSession(const SessionId& sessionId) const;
    
    /**
     * Get list of all active sessions
     * @return vector of session identifiers
     */
    std::vector<SessionId> getActiveSessions() const;
    
    /**
     * Set output callback
     * @param callback function to process session output
     */
    void setOutputCallback(OutputCallback callback);
    
    /**
     * Set status change callback
     * @param callback function to process session status changes
     */
    void setStatusCallback(StatusCallback callback);
    
    /**
     * Get statistics
     */
    struct Stats {
        size_t totalSessions;
        size_t activeSessions;
        size_t pollCycles;
        double avgPollTimeMs;
    };
    
    Stats getStats() const;

private:
    /**
     * Main session polling loop
     */
    void pollLoop();
    
    /**
     * Poll single session
     * @param sessionId session identifier
     * @param session session pointer
     */
    void pollSession(const SessionId& sessionId, std::shared_ptr<TerminalSessionController> session);
    
    /**
     * Cleanup finished sessions
     */
    void cleanupFinishedSessions();

private:
    mutable std::mutex sessionsMutex;
    std::unordered_map<SessionId, std::shared_ptr<TerminalSessionController>> sessions;
    
    std::atomic<bool> running{false};
    std::unique_ptr<std::thread> pollThread;
    int pollIntervalMs;
    
    OutputCallback outputCallback;
    StatusCallback statusCallback;
    
    // Statistics
    mutable std::mutex statsMutex;
    Stats stats{0, 0, 0, 0.0};
    
    // TODO: Add in future:
    // - std::chrono::steady_clock::time_point m_lastCleanup; // Time of last cleanup
    // - size_t m_maxSessions = 100; // Session limit
    // - std::unordered_map<SessionId, std::chrono::steady_clock::time_point> m_sessionActivity; // Session activity
    // - bool m_autoCleanup = true; // Auto cleanup of inactive sessions
    // - std::chrono::seconds m_sessionTimeout{3600}; // Session timeout
}; 