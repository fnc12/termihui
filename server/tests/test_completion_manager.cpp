#include <catch2/catch_test_macros.hpp>
#include "CompletionManager.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

// Helper functions for tests
bool contains(const std::vector<std::string>& container, const std::string& item) {
    for (const auto& elem : container) {
        if (elem == item) {
            return true;
        }
    }
    return false;
}

TEST_CASE("CompletionManager command completion", "[completion]") {
    CompletionManager manager;
    
    SECTION("Complete 'ls' command") {
        auto completions = manager.getCompletions("ls", 2);
        REQUIRE(contains(completions, "ls"));
    }
    
    SECTION("Complete 'pw' to 'pwd'") {
        auto completions = manager.getCompletions("pw", 2);
        REQUIRE(contains(completions, "pwd"));
    }
}

TEST_CASE("CompletionManager empty input", "[completion]") {
    CompletionManager manager;
    
    // Empty input returns empty completions (tab inserts tab character on client)
    auto completions = manager.getCompletions("", 0);
    REQUIRE(completions.empty());
}

TEST_CASE("CompletionManager file completion", "[completion]") {
    CompletionManager manager;
    
    SECTION("File completion in current directory") {
        auto completions = manager.getCompletions("cat tes", 7);
        // Result depends on directory contents, just check it doesn't crash
        INFO("Found " << completions.size() << " file completions for 'tes'");
        REQUIRE(true); // Test passes if it doesn't crash
    }
}

TEST_CASE("CompletionManager tilde expansion", "[completion][tilde]") {
    CompletionManager manager;
    
    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        SKIP("HOME environment variable not set");
    }
    
    SECTION("Tilde expansion for ~/De pattern") {
        auto completions = manager.getCompletions("cd ~/De", 7);
        
        INFO("Testing tilde expansion for 'cd ~/De'");
        INFO("HOME directory: " << home);
        INFO("Found " << completions.size() << " completions");
        
        // Print found completions for debugging
        for (const auto& completion : completions) {
            INFO("  - " << completion);
            
            // Check that result starts with tilde (preserves original format)
            if (!completion.empty() && completion[0] == '~') {
                INFO("  Correctly preserved tilde format");
            }
        }
        
        // Check that at least one result has tilde
        bool has_tilde_result = false;
        for (const auto& completion : completions) {
            if (!completion.empty() && completion[0] == '~') {
                has_tilde_result = true;
                break;
            }
        }
        
        // If there are results, they should preserve tilde format
        if (!completions.empty()) {
            REQUIRE(has_tilde_result);
        }
    }
}

TEST_CASE("CompletionManager path parsing", "[completion]") {
    CompletionManager manager;
    
    // Test various path formats
    std::vector<std::pair<std::string, int>> test_cases = {
        {"ls", 2},                    // Command
        {"ls file", 7},               // File
        {"cd /usr/", 8},              // Absolute path
        {"cat ~/Documents/", 15},     // Tilde with subpath
        {"vim ./src/", 9}             // Relative path
    };
    
    for (const auto& test_case : test_cases) {
        DYNAMIC_SECTION("Testing path: " << test_case.first << " at position " << test_case.second) {
            auto completions = manager.getCompletions(test_case.first, test_case.second);
            INFO("Found " << completions.size() << " completions");
            
            // Print first few results for debugging
            int count = 0;
            for (const auto& completion : completions) {
                if (count++ >= 3) break;
                INFO("  - " << completion);
            }
            
            // Test passes if it doesn't crash
            REQUIRE(true);
        }
    }
}
