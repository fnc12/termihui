#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include "SessionStorageSchema.h"

class SessionStorage {
public:
    explicit SessionStorage(std::filesystem::path dbPath);

    void initialize();
    
    // Add new command and return its id
    uint64_t addCommand(uint64_t serverRunId, const std::string& command, const std::string& cwdStart);
    
    // Append output to existing command
    void appendOutput(uint64_t commandId, const std::string& output);
    
    // Finish command with exit code and final cwd
    void finishCommand(uint64_t commandId, int exitCode, const std::string& cwdEnd);
    
    // Get command by id
    std::optional<SessionCommand> getCommand(uint64_t commandId);
    
    // Get all commands
    std::vector<SessionCommand> getAllCommands();
    
    // Get last known cwd from finished commands (for session restoration)
    std::optional<std::string> getLastCwd();
    
    // Rendered output lines (stored as pre-serialized JSON for client passthrough)
    void addOutputLine(uint64_t commandId, std::string segmentsJson);
    std::vector<std::string> getOutputLines(uint64_t commandId);

private:
    std::filesystem::path dbPath;
    SessionStorageType storage;
};
