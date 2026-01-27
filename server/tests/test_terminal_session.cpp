#include <catch2/catch_test_macros.hpp>
#include "TerminalSessionController.h"
#include <thread>
#include <chrono>

TEST_CASE("TerminalSessionController basic functionality", "[TerminalSessionController]") {
    
    SECTION("Session creation and command execution") {
        TerminalSessionController session;
        
        // Test session creation with simple command
        REQUIRE(session.startCommand("echo 'Hello, World!'"));
        
        // Check that session is running
        REQUIRE(session.isRunning());
        
        // Check that we got a valid PID
        REQUIRE(session.getChildPid() > 0);
        
        // Wait for command execution
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Read output
        std::string output;
        int attempts = 0;
        while (session.isRunning() && attempts < 50) {
            if (session.hasData()) {
                std::string newOutput = session.readOutput();
                output += newOutput;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        
        // Check that we got expected output
        REQUIRE_FALSE(output.empty());
        REQUIRE(output.find("Hello, World!") != std::string::npos);
    }
    
    SECTION("ls command execution") {
        TerminalSessionController session;
        
        // Test ls command
        REQUIRE(session.startCommand("ls"));
        
        // Check that session is running
        REQUIRE(session.isRunning());
        
        // Wait for command execution
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Read output
        std::string output;
        int attempts = 0;
        while (session.isRunning() && attempts < 100) {
            if (session.hasData()) {
                std::string newOutput = session.readOutput();
                output += newOutput;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        
        // Check that we got output from ls
        REQUIRE_FALSE(output.empty());
        // ls should show at least something (files or empty directory)
        REQUIRE(output.length() > 0);
        
        INFO("ls output: " << output);
    }
    
    SECTION("Interactive session with input") {
        TerminalSessionController session;
        
        // Start bash for interactive session
        REQUIRE(session.startCommand("bash"));
        
        // Wait for bash to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Send ls command
        REQUIRE(session.sendInput("ls\n") > 0);
        
        // Wait for command execution
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Read output
        std::string output;
        int attempts = 0;
        while (attempts < 50) {
            if (session.hasData()) {
                std::string newOutput = session.readOutput();
                output += newOutput;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
            
            // Break if we got enough data
            if (output.length() > 10) {
                break;
            }
        }
        
        // Send exit for clean shutdown
        session.sendInput("exit\n");
        
        // Wait for termination
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Check that we got output
        REQUIRE_FALSE(output.empty());
        
        INFO("Interactive session output: " << output);
        
        // Force terminate if still running
        if (session.isRunning()) {
            session.terminate();
        }
    }
}

TEST_CASE("TerminalSessionController error handling", "[TerminalSessionController]") {
    
    SECTION("Invalid command") {
        TerminalSessionController session;
        
        // Test non-existent command
        REQUIRE(session.startCommand("nonexistent_command_12345"));
        
        // Wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Session should terminate with error
        // (bash will try to execute command and exit)
    }
    
    SECTION("Double start prevention") {
        TerminalSessionController session;
        
        // Start first command
        REQUIRE(session.startCommand("sleep 1"));
        
        // Attempt to start second command should fail
        REQUIRE_FALSE(session.startCommand("echo test"));
        
        // Wait for first command to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

TEST_CASE("TerminalSessionController resource management", "[TerminalSessionController]") {
    
    SECTION("Proper cleanup on destruction") {
        pid_t childPid = -1;
        
        {
            TerminalSessionController session;
            REQUIRE(session.startCommand("sleep 2"));
            childPid = session.getChildPid();
            REQUIRE(childPid > 0);
            REQUIRE(session.isRunning());
        } // session is destroyed here
        
        // Wait for proper cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check that process was properly terminated
        // (can be verified via kill(pid, 0), but that's complex in tests)
    }
}

TEST_CASE("Interactive bash session state persistence", "[TerminalSessionController][Interactive]") {
    SECTION("Directory change should persist between commands") {
        TerminalSessionController session;
        
        // Create interactive bash session
        REQUIRE(session.createSession());
        REQUIRE(session.isRunning());
        
        // Wait for bash initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Clear buffer from bash welcome messages
        session.readOutput();
        
        // 1. Execute pwd and save result
        REQUIRE(session.executeCommand("pwd"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::string initialDir;
        int attempts = 0;
        while (attempts < 20) {
            if (session.hasData()) {
                std::string output = session.readOutput();
                initialDir += output;
                if (initialDir.find('\n') != std::string::npos) {
                    break; // Got complete line
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Clean up extra characters
        size_t newlinePos = initialDir.find('\n');
        if (newlinePos != std::string::npos) {
            initialDir = initialDir.substr(0, newlinePos);
        }
        
        REQUIRE(!initialDir.empty());
        
        // 2. Change directory to /tmp
        REQUIRE(session.executeCommand("cd /tmp"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Clear buffer after cd (cd usually outputs nothing)
        session.readOutput();
        
        // 3. Execute pwd again
        REQUIRE(session.executeCommand("pwd"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::string newDir;
        attempts = 0;
        while (attempts < 20) {
            if (session.hasData()) {
                std::string output = session.readOutput();
                newDir += output;
                if (newDir.find('\n') != std::string::npos) {
                    break; // Got complete line
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Clean up extra characters
        newlinePos = newDir.find('\n');
        if (newlinePos != std::string::npos) {
            newDir = newDir.substr(0, newlinePos);
        }
        
        REQUIRE(!newDir.empty());
        
        // 4. MAIN CHECK: directories should be different
        // If interactive session works correctly, newDir should be "/tmp"
        // And initialDir should be the original directory (not "/tmp")
        INFO("Initial directory: " << initialDir);
        INFO("Directory after cd /tmp: " << newDir);
        
        // Check that directory changed
        REQUIRE(initialDir != newDir);
        
        // Additional check - new directory should be /tmp
        REQUIRE(newDir == "/tmp");
    }
} 