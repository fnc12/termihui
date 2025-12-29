#pragma once

#include "ServerStorageModels.h"
#include <filesystem>
#include <optional>

class ServerStorage {
public:
    explicit ServerStorage(const std::filesystem::path& dbPath);
    
    uint64_t recordStart();
    void recordStop(uint64_t runId);
    
    std::optional<ServerRun> getLastRun();
    std::optional<ServerStop> getStopForRun(uint64_t runId);
    bool wasLastRunCrashed();

private:
    ServerStorageType storage;
};
