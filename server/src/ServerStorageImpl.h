#pragma once

#include "ServerStorage.h"
#include "ServerStorageModels.h"
#include <filesystem>

/**
 * SQLite implementation of ServerStorage
 */
class ServerStorageImpl : public ServerStorage {
public:
    explicit ServerStorageImpl(const std::filesystem::path& dbPath);
    
    // Server run tracking
    uint64_t recordStart() override;
    void recordStop(uint64_t runId) override;
    std::optional<ServerRun> getLastRun() override;
    std::optional<ServerStop> getStopForRun(uint64_t runId) override;
    bool wasLastRunCrashed() override;
    
    // Terminal session management
    uint64_t createTerminalSession(uint64_t serverRunId) override;
    void markTerminalSessionAsDeleted(uint64_t sessionId) override;
    bool isActiveTerminalSession(uint64_t sessionId) override;
    std::optional<TerminalSession> getTerminalSession(uint64_t sessionId) override;
    std::vector<TerminalSession> getActiveTerminalSessions() override;
    
    // LLM Provider management
    uint64_t addLLMProvider(const std::string& name, const std::string& type,
                            const std::string& url, const std::string& model,
                            const std::string& apiKey) override;
    void updateLLMProvider(uint64_t id, const std::string& name,
                           const std::string& url, const std::string& model,
                           const std::string& apiKey) override;
    void deleteLLMProvider(uint64_t id) override;
    std::optional<LLMProvider> getLLMProvider(uint64_t id) override;
    std::vector<LLMProvider> getAllLLMProviders() override;
    
    // Chat history management
    uint64_t saveChatMessage(uint64_t sessionId, const std::string& role, const std::string& content) override;
    std::vector<ChatMessageRecord> getChatHistory(uint64_t sessionId) override;
    void clearChatHistory(uint64_t sessionId) override;

private:
    ServerStorageType storage;
};
