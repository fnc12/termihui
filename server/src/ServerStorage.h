#pragma once

#include "ServerStorageModels.h"
#include <filesystem>
#include <optional>
#include <vector>

class ServerStorage {
public:
    explicit ServerStorage(const std::filesystem::path& dbPath);
    
    // Server run tracking
    uint64_t recordStart();
    void recordStop(uint64_t runId);
    std::optional<ServerRun> getLastRun();
    std::optional<ServerStop> getStopForRun(uint64_t runId);
    bool wasLastRunCrashed();
    
    // Terminal session management
    uint64_t createTerminalSession(uint64_t serverRunId);
    void markTerminalSessionAsDeleted(uint64_t sessionId);
    bool isActiveTerminalSession(uint64_t sessionId);
    std::optional<TerminalSession> getTerminalSession(uint64_t sessionId);
    std::vector<TerminalSession> getActiveTerminalSessions();
    
    // LLM Provider management
    uint64_t addLLMProvider(const std::string& name, const std::string& type,
                            const std::string& url, const std::string& model,
                            const std::string& apiKey);
    void updateLLMProvider(uint64_t id, const std::string& name,
                           const std::string& url, const std::string& model,
                           const std::string& apiKey);
    void deleteLLMProvider(uint64_t id);
    std::optional<LLMProvider> getLLMProvider(uint64_t id);
    std::vector<LLMProvider> getAllLLMProviders();

private:
    ServerStorageType storage;
};
