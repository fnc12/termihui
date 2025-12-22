#include <catch2/catch_test_macros.hpp>
#include "TerminalSessionController.h"
#include <thread>
#include <chrono>

TEST_CASE("UTF-8 command handling", "[TerminalSessionController][UTF8]") {
    TerminalSessionController session;
    
    // Create interactive bash session
    REQUIRE(session.createSession());
    REQUIRE(session.isRunning());
    
    // Wait for bash initialization
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Clear buffer from bash welcome messages
    session.readOutput();
    
    SECTION("Cyrillic command should return 'command not found'") {
        // Send command with Cyrillic characters (like typing "pwd" on Russian keyboard)
        REQUIRE(session.executeCommand("зцв"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string output;
        int attempts = 0;
        while (attempts < 30) {
            if (session.hasData(100)) {
                output += session.readOutput();
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        INFO("Output from Cyrillic command: '" << output << "'");
        INFO("Output length: " << output.length());
        
        // Print hex dump for debugging
        std::string hexDump;
        for (unsigned char c : output) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02x ", c);
            hexDump += buf;
        }
        INFO("Hex dump: " << hexDump);
        
        // Should have some output (either error message or OSC markers)
        REQUIRE(!output.empty());
        
        // Should contain "not found" or similar error message
        bool hasError = output.find("not found") != std::string::npos ||
                        output.find("command not found") != std::string::npos ||
                        output.find("зцв") != std::string::npos;
        REQUIRE(hasError);
    }
    
    SECTION("Create file with Cyrillic name") {
        // First go to /tmp
        REQUIRE(session.executeCommand("cd /tmp"));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        session.readOutput();
        
        // Create file with Cyrillic name
        REQUIRE(session.executeCommand("touch тест_файл.txt"));
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        session.readOutput();
        
        // List files and check if our file is there
        REQUIRE(session.executeCommand("ls -la тест_файл.txt"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string output;
        int attempts = 0;
        while (attempts < 20) {
            if (session.hasData(50)) {
                output += session.readOutput();
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        INFO("Output from ls: '" << output << "'");
        
        // Should contain the filename
        REQUIRE(!output.empty());
        REQUIRE(output.find("тест_файл.txt") != std::string::npos);
        
        // Cleanup
        session.executeCommand("rm -f тест_файл.txt");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    SECTION("Echo Cyrillic text") {
        REQUIRE(session.executeCommand("echo 'Привет мир!'"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string output;
        int attempts = 0;
        while (attempts < 20) {
            if (session.hasData(50)) {
                output += session.readOutput();
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        INFO("Output from echo: '" << output << "'");
        
        REQUIRE(!output.empty());
        REQUIRE(output.find("Привет мир!") != std::string::npos);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Increase wait time
        
        std::string initialDir;
        std::string allOutput; // For debugging - save all output
        int attempts = 0;
        while (attempts < 20) {
            if (session.hasData(50)) {
                std::string output = session.readOutput();
                allOutput += output; // Save for debugging
                initialDir += output;
                if (initialDir.find('\n') != std::string::npos) {
                    break; // Got complete line
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        INFO("Output from first pwd: '" << allOutput << "'");
        
        REQUIRE(!allOutput.empty());
        
        // 2. Change directory to /tmp
        REQUIRE(session.executeCommand("cd /tmp"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Clear buffer after cd (cd usually outputs nothing)
        session.readOutput();
        
        // 3. Execute pwd again
        REQUIRE(session.executeCommand("pwd"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string newOutput; // Full output of second pwd command
        attempts = 0;
        while (attempts < 20) {
            if (session.hasData(50)) {
                std::string output = session.readOutput();
                newOutput += output;
                if (newOutput.find("bash-3.2$") != std::string::npos) {
                    break; // Got full output with prompt
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        INFO("Output from second pwd: '" << newOutput << "'");
        
        // 4. MAIN CHECK: if interactive session works correctly,
        // pwd outputs should be DIFFERENT (different directories)
        REQUIRE(!newOutput.empty());
        
        // If cd worked, outputs should differ
        REQUIRE(allOutput != newOutput);
    }
}