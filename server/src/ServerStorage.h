#pragma once

#include "ServerStorageModels.h"
#include <optional>
#include <vector>
#include <string>

/**
 * Abstract interface for server storage
 * Handles persistence of server state, sessions, LLM providers, and chat history
 */
class ServerStorage {
public:
    virtual ~ServerStorage() = default;
    
    // Server run tracking
    virtual uint64_t recordStart() = 0;
    virtual void recordStop(uint64_t runId) = 0;
    virtual std::optional<ServerRun> getLastRun() = 0;
    virtual std::optional<ServerStop> getStopForRun(uint64_t runId) = 0;
    virtual bool wasLastRunCrashed() = 0;
    
    // Terminal session management
    virtual uint64_t createTerminalSession(uint64_t serverRunId) = 0;
    virtual void markTerminalSessionAsDeleted(uint64_t sessionId) = 0;
    virtual bool isActiveTerminalSession(uint64_t sessionId) = 0;
    virtual std::optional<TerminalSession> getTerminalSession(uint64_t sessionId) = 0;
    virtual std::vector<TerminalSession> getActiveTerminalSessions() = 0;
    
    // LLM Provider management
    virtual uint64_t addLLMProvider(const std::string& name, const std::string& type,
                                    const std::string& url, const std::string& model,
                                    const std::string& apiKey) = 0;
    virtual void updateLLMProvider(uint64_t id, const std::string& name,
                                   const std::string& url, const std::string& model,
                                   const std::string& apiKey) = 0;
    virtual void deleteLLMProvider(uint64_t id) = 0;
    virtual std::optional<LLMProvider> getLLMProvider(uint64_t id) = 0;
    virtual std::vector<LLMProvider> getAllLLMProviders() = 0;
    
    // Chat history management
    virtual uint64_t saveChatMessage(uint64_t sessionId, const std::string& role, const std::string& content) = 0;
    virtual std::vector<ChatMessageRecord> getChatHistory(uint64_t sessionId) = 0;
    virtual void clearChatHistory(uint64_t sessionId) = 0;
};
