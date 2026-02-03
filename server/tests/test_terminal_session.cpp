#include <catch2/catch_test_macros.hpp>
#include "TerminalSessionController.h"
#include <thread>
#include <chrono>
#include <filesystem>

// =============================================================================
// BUG: Session should start in correct cwd, not inherit from parent process
// =============================================================================

TEST_CASE("New session starts in HOME directory", "[TerminalSessionController][cwd]") {
    auto dbPath = std::filesystem::temp_directory_path() / "test_new_session_cwd.sqlite";
    std::filesystem::remove(dbPath);  // Clean DB - no history
    
    TerminalSessionController session(dbPath, 1, 1);
    REQUIRE(session.createSession());
    REQUIRE(session.isRunning());
    
    // Wait for bash initialization
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    session.readOutput();  // clear buffer
    
    // Execute pwd
    auto result = session.executeCommand("pwd");
    REQUIRE(result.isOk());
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    std::string output;
    int attempts = 0;
    while (attempts < 30) {
        if (session.hasData()) {
            output += session.readOutput();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        attempts++;
    }
    
    INFO("Output: " << output);
    
    const char* home = getenv("HOME");
    REQUIRE(home != nullptr);
    
    // Session should start in HOME
    REQUIRE(output.find(home) != std::string::npos);
}

TEST_CASE("Restored session starts in last cwd from history", "[TerminalSessionController][cwd]") {
    auto dbPath = std::filesystem::temp_directory_path() / "test_restore_cwd.sqlite";
    std::filesystem::remove(dbPath);  // Clean start
    
    // Phase 1: Create session, record history with /tmp as final cwd
    {
        TerminalSessionController session(dbPath, 1, 1);
        REQUIRE(session.createSession());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        session.readOutput();
        
        // Record command in history (simulating OSC 133 markers)
        session.setPendingCommand("cd /tmp");
        session.startCommandInHistory("/some/initial/path");
        session.finishCurrentCommand(0, "/tmp");
        
        session.terminate();
    }
    
    // Phase 2: Create new session with same dbPath (simulates server restart)
    {
        TerminalSessionController session(dbPath, 1, 2);  // new serverRunId
        REQUIRE(session.createSession());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        session.readOutput();
        
        // Execute pwd
        auto result = session.executeCommand("pwd");
        REQUIRE(result.isOk());
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        
        std::string output;
        int attempts = 0;
        while (attempts < 30) {
            if (session.hasData()) {
                output += session.readOutput();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            attempts++;
        }
        
        INFO("Output: " << output);
        
        // Should contain /tmp (or /private/tmp on macOS)
        REQUIRE((output.find("/tmp") != std::string::npos));
    }
    
    std::filesystem::remove(dbPath);
}
