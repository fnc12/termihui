#pragma once

#include "TerminalSessionController.h"
#include <variant>
#include <vector>
#include <string>
#include <queue>
#include <filesystem>

/**
 * Mock TerminalSessionController for unit tests
 * Records calls and returns configured test data
 */
class TerminalSessionControllerMock : public TerminalSessionController {
public:
    // Call record structures with default comparison (C++20)
    struct SetLastKnownCwdCall {
        std::string cwd;
        auto operator<=>(const SetLastKnownCwdCall&) const = default;
    };
    
    struct StartCommandInHistoryCall {
        std::string cwd;
        auto operator<=>(const StartCommandInHistoryCall&) const = default;
    };
    
    struct AppendOutputToCurrentCommandCall {
        std::string output;
        auto operator<=>(const AppendOutputToCurrentCommandCall&) const = default;
    };
    
    struct FinishCurrentCommandCall {
        int exitCode = 0;
        std::string cwd;
        auto operator<=>(const FinishCurrentCommandCall&) const = default;
    };
    
    using Call = std::variant<
        SetLastKnownCwdCall,
        StartCommandInHistoryCall,
        AppendOutputToCurrentCommandCall,
        FinishCurrentCommandCall
    >;
    
    TerminalSessionControllerMock() 
        : TerminalSessionController(std::filesystem::temp_directory_path() / "test_mock.sqlite", 1, 1) {}
    
    // Recorded calls
    std::vector<Call> calls;
    
    // Configurable return values
    bool hasDataReturnValue = true;
    std::queue<std::string> readOutputReturnValues;
    bool hasActiveCommandReturnValue = true;
    
    // Override methods for mocking
    bool hasData() const override {
        return this->hasDataReturnValue;
    }
    
    std::string readOutput() override {
        if (this->readOutputReturnValues.empty()) {
            this->hasDataReturnValue = false;
            return "";
        }
        std::string result = this->readOutputReturnValues.front();
        this->readOutputReturnValues.pop();
        if (this->readOutputReturnValues.empty()) {
            this->hasDataReturnValue = false;
        }
        return result;
    }
    
    void setLastKnownCwd(const std::string& cwd) override {
        this->calls.push_back(SetLastKnownCwdCall{cwd});
    }
    
    void startCommandInHistory(const std::string& cwd) override {
        this->calls.push_back(StartCommandInHistoryCall{cwd});
    }
    
    void appendOutputToCurrentCommand(const std::string& output) override {
        this->calls.push_back(AppendOutputToCurrentCommandCall{output});
    }
    
    void finishCurrentCommand(int exitCode, const std::string& cwd) override {
        this->calls.push_back(FinishCurrentCommandCall{exitCode, cwd});
    }
    
    bool hasActiveCommand() const override {
        return this->hasActiveCommandReturnValue;
    }
};

