#pragma once

#include <cstdint>
#include <string>

// Data model for session commands
struct SessionCommand {
    uint64_t id = 0;
    uint64_t serverRunId = 0;
    std::string command;
    std::string output;
    int exitCode = 0;
    std::string cwdStart;
    std::string cwdEnd;
    bool isFinished = false;
    long long timestamp = 0; // Unix timestamp
};

