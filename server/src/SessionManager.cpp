#include "SessionManager.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <fmt/core.h>

SessionManager::SessionManager(int pollIntervalMs)
    : pollIntervalMs(pollIntervalMs)
{
}

SessionManager::~SessionManager()
{
    stop();
}

void SessionManager::start()
{
    if (this->running.exchange(true)) {
        return; // Already running
    }
    
    this->pollThread = std::make_unique<std::thread>(&SessionManager::pollLoop, this);
    fmt::print("SessionManager started with interval {}ms\n", this->pollIntervalMs);
}

void SessionManager::stop()
{
    if (!this->running.exchange(false)) {
        return; // Already stopped
    }
    
    if (this->pollThread && this->pollThread->joinable()) {
        this->pollThread->join();
    }
    
    // Close all sessions
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    for (auto& [sessionId, session] : this->sessions) {
        session->terminate();
    }
    this->sessions.clear();
    
    fmt::print("SessionManager stopped\n");
}

bool SessionManager::createSession(const SessionId& sessionId, const std::string& command)
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    // Check if session already exists
    if (this->sessions.find(sessionId) != this->sessions.end()) {
        fmt::print(stderr, "Session with ID '{}' already exists\n", sessionId);
        return false;
    }
    
    // Create new session
    auto session = std::make_shared<TerminalSessionController>();
    if (!session->createSession()) {
        fmt::print(stderr, "Failed to create interactive session for '{}'\n", sessionId);
        return false;
    }
    
    // Execute first command
    if (!session->executeCommand(command)) {
        fmt::print(stderr, "Failed to execute command '{}' for session '{}'\n", command, sessionId);
        return false;
    }
    
    this->sessions[sessionId] = session;
    
    // Update statistics
    {
        std::lock_guard<std::mutex> statsLock(this->statsMutex);
        this->stats.totalSessions++;
        this->stats.activeSessions++;
    }
    
    fmt::print("Created session '{}' (PID: {})\n", sessionId, session->getChildPid());
    return true;
}

bool SessionManager::closeSession(const SessionId& sessionId)
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    auto it = this->sessions.find(sessionId);
    if (it == this->sessions.end()) {
        return false;
    }
    
    // Terminate session
    it->second->terminate();
    this->sessions.erase(it);
    
    // Update statistics
    {
        std::lock_guard<std::mutex> statsLock(this->statsMutex);
        if (this->stats.activeSessions > 0) {
            this->stats.activeSessions--;
        }
    }
    
    fmt::print("Closed session '{}'\n", sessionId);
    return true;
}

ssize_t SessionManager::sendInput(const SessionId& sessionId, const std::string& input)
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    auto it = this->sessions.find(sessionId);
    if (it == this->sessions.end()) {
        return -1;
    }
    
    return it->second->sendInput(input);
}

bool SessionManager::hasSession(const SessionId& sessionId) const
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    return this->sessions.find(sessionId) != this->sessions.end();
}

std::vector<SessionManager::SessionId> SessionManager::getActiveSessions() const
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    std::vector<SessionId> result;
    result.reserve(this->sessions.size());
    
    for (const auto& [sessionId, session] : this->sessions) {
        if (session->isRunning()) {
            result.push_back(sessionId);
        }
    }
    
    return result;
}

void SessionManager::setOutputCallback(OutputCallback callback)
{
    this->outputCallback = std::move(callback);
}

void SessionManager::setStatusCallback(StatusCallback callback)
{
    this->statusCallback = std::move(callback);
}

SessionManager::Stats SessionManager::getStats() const
{
    std::lock_guard<std::mutex> lock(this->statsMutex);
    return this->stats;
}

void SessionManager::pollLoop()
{
    fmt::print("Session polling loop started\n");
    
    while (this->running.load()) {
        auto startTime = std::chrono::steady_clock::now();
        
        // Copy session list for safe iteration
        std::unordered_map<SessionId, std::shared_ptr<TerminalSessionController>> sessionsCopy;
        {
            std::lock_guard<std::mutex> lock(this->sessionsMutex);
            sessionsCopy = this->sessions;
        }
        
        // Poll all sessions
        for (const auto& [sessionId, session] : sessionsCopy) {
            pollSession(sessionId, session);
        }
        
        // Cleanup finished sessions
        cleanupFinishedSessions();
        
        // Update statistics
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> statsLock(this->statsMutex);
            this->stats.pollCycles++;
            this->stats.avgPollTimeMs = (this->stats.avgPollTimeMs * (this->stats.pollCycles - 1) + duration.count() / 1000.0) / this->stats.pollCycles;
        }
        
        // Wait until next cycle
        std::this_thread::sleep_for(std::chrono::milliseconds(this->pollIntervalMs));
    }
    
    fmt::print("Session polling loop completed\n");
}

void SessionManager::pollSession(const SessionId& sessionId, std::shared_ptr<TerminalSessionController> session)
{
    if (!session->isRunning()) {
        return;
    }
    
    // Read output if available
    if (session->hasData()) { // Non-blocking check
        std::string output = session->readOutput();
        if (!output.empty() && this->outputCallback) {
            this->outputCallback(sessionId, output);
        }
    }
    
    // Check status change
    static thread_local std::unordered_map<SessionId, bool> lastStatus;
    bool currentStatus = session->isRunning();
    
    if (lastStatus[sessionId] != currentStatus) {
        lastStatus[sessionId] = currentStatus;
        if (this->statusCallback) {
            this->statusCallback(sessionId, currentStatus);
        }
    }
}

void SessionManager::cleanupFinishedSessions()
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    auto it = this->sessions.begin();
    while (it != this->sessions.end()) {
        if (!it->second->isRunning()) {
            fmt::print("Removing finished session '{}'\n", it->first);
            
            // Update statistics
            {
                std::lock_guard<std::mutex> statsLock(this->statsMutex);
                if (this->stats.activeSessions > 0) {
                    this->stats.activeSessions--;
                }
            }
            
            it = this->sessions.erase(it);
        } else {
            ++it;
        }
    }
} 