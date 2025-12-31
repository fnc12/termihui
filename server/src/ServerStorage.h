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
    std::optional<TerminalSession> getTerminalSession(uint64_t sessionId);
    std::vector<TerminalSession> getActiveTerminalSessions();

private:
    ServerStorageType storage;
};
