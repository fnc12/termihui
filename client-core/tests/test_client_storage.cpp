#include <catch2/catch_test_macros.hpp>
#include "termihui/client_storage.h"
#include <filesystem>
#include <cstdio>

// Helper to create temporary database path
class TempDatabase {
public:
    TempDatabase() : path(std::tmpnam(nullptr)) {
        path += ".sqlite";
    }
    ~TempDatabase() {
        std::filesystem::remove(path);
    }
    const std::filesystem::path& getPath() const { return path; }
private:
    std::filesystem::path path;
};

TEST_CASE("ClientStorage - getByCommandId", "[ClientStorage]") {
    TempDatabase tempDb;
    ClientStorage storage(tempDb.getPath());
    
    SECTION("same commandId in different sessions returns correct block") {
        // This test verifies that getByCommandId correctly filters by sessionId
        // because commandId is not unique across sessions (server assigns IDs per-session starting from 1)
        
        // Session 1: command with id=1 is "echo session1" with output "session1"
        CommandBlock session1Block;
        session1Block.sessionId = 1;
        session1Block.commandId = 1;  // Same commandId as in session 3!
        session1Block.command = "echo session1";
        session1Block.output = "session1";
        session1Block.isFinished = true;
        
        // Session 3: command with id=1 is "pwd" with output "/Users/yevgeniyzakharov"
        CommandBlock session3Block;
        session3Block.sessionId = 3;
        session3Block.commandId = 1;  // Same commandId as in session 1!
        session3Block.command = "pwd";
        session3Block.output = "/Users/yevgeniyzakharov";
        session3Block.isFinished = true;
        
        storage.insertCommandBlock(session1Block);
        storage.insertCommandBlock(session3Block);
        
        // When user is in Session 3 and copies block with commandId=1,
        // they should get the correct block from session 3
        auto result = storage.getByCommandId(1, 3);
        REQUIRE(result.has_value());
        REQUIRE(result->command == "pwd");
        REQUIRE(result->output == "/Users/yevgeniyzakharov");
        REQUIRE(result->sessionId == 3);
        
        // Similarly, querying for session 1 should return session1Block
        auto result1 = storage.getByCommandId(1, 1);
        REQUIRE(result1.has_value());
        REQUIRE(result1->command == "echo session1");
        REQUIRE(result1->output == "session1");
        REQUIRE(result1->sessionId == 1);
    }
    
    SECTION("returns correct block when multiple blocks exist") {
        // Simulate loading history from server with 3 commands
        // Server command IDs: 10, 11, 12
        uint64_t sessionId = 1;
        
        CommandBlock block1;
        block1.sessionId = sessionId;
        block1.commandId = 10;
        block1.command = "pwd";
        block1.output = "/Users/yevgeniyzakharov";
        block1.isFinished = true;
        
        CommandBlock block2;
        block2.sessionId = sessionId;
        block2.commandId = 11;
        block2.command = "curl -I 192.168.68.111:11440";
        block2.output = "HTTP/1.1 200 OK\nContent-Type: text/html";
        block2.isFinished = true;
        
        CommandBlock block3;
        block3.sessionId = sessionId;
        block3.commandId = 12;
        block3.command = "pwd";
        block3.output = "/Users/yevgeniyzakharov";
        block3.isFinished = true;
        
        // Insert blocks (simulating history load)
        int64_t localId1 = storage.insertCommandBlock(block1);
        int64_t localId2 = storage.insertCommandBlock(block2);
        int64_t localId3 = storage.insertCommandBlock(block3);
        
        REQUIRE(localId1 == 1);
        REQUIRE(localId2 == 2);
        REQUIRE(localId3 == 3);
        
        // Now test getByCommandId - this should return the correct block
        SECTION("getByCommandId(10, sessionId) returns block1") {
            auto result = storage.getByCommandId(10, sessionId);
            REQUIRE(result.has_value());
            REQUIRE(result->command == "pwd");
            REQUIRE(result->output == "/Users/yevgeniyzakharov");
            REQUIRE(result->commandId.has_value());
            REQUIRE(*result->commandId == 10);
        }
        
        SECTION("getByCommandId(11, sessionId) returns block2") {
            auto result = storage.getByCommandId(11, sessionId);
            REQUIRE(result.has_value());
            REQUIRE(result->command == "curl -I 192.168.68.111:11440");
            REQUIRE(result->output == "HTTP/1.1 200 OK\nContent-Type: text/html");
            REQUIRE(result->commandId.has_value());
            REQUIRE(*result->commandId == 11);
        }
        
        SECTION("getByCommandId(12, sessionId) returns block3") {
            auto result = storage.getByCommandId(12, sessionId);
            REQUIRE(result.has_value());
            REQUIRE(result->command == "pwd");
            REQUIRE(result->output == "/Users/yevgeniyzakharov");
            REQUIRE(result->commandId.has_value());
            REQUIRE(*result->commandId == 12);
        }
        
        SECTION("getByCommandId for non-existent ID returns nullopt") {
            auto result = storage.getByCommandId(999, sessionId);
            REQUIRE_FALSE(result.has_value());
        }
    }
    
    SECTION("handles blocks with null commandId (in-progress commands)") {
        uint64_t sessionId = 1;
        
        // Block without commandId (in-progress)
        CommandBlock inProgressBlock;
        inProgressBlock.sessionId = sessionId;
        inProgressBlock.command = "sleep 10";
        inProgressBlock.output = "";
        inProgressBlock.isFinished = false;
        // commandId is not set (std::nullopt)
        
        // Block with commandId
        CommandBlock finishedBlock;
        finishedBlock.sessionId = sessionId;
        finishedBlock.commandId = 100;
        finishedBlock.command = "echo hello";
        finishedBlock.output = "hello";
        finishedBlock.isFinished = true;
        
        storage.insertCommandBlock(inProgressBlock);
        storage.insertCommandBlock(finishedBlock);
        
        // Search for commandId=100 should return finishedBlock, not inProgressBlock
        auto result = storage.getByCommandId(100, sessionId);
        REQUIRE(result.has_value());
        REQUIRE(result->command == "echo hello");
        REQUIRE(result->output == "hello");
    }
    
    SECTION("clearSession removes blocks only for specified session") {
        // Session 1 blocks
        CommandBlock s1b1;
        s1b1.sessionId = 1;
        s1b1.commandId = 10;
        s1b1.command = "ls";
        s1b1.output = "file1\nfile2";
        s1b1.isFinished = true;
        
        // Session 2 blocks  
        CommandBlock s2b1;
        s2b1.sessionId = 2;
        s2b1.commandId = 20;
        s2b1.command = "pwd";
        s2b1.output = "/home";
        s2b1.isFinished = true;
        
        storage.insertCommandBlock(s1b1);
        storage.insertCommandBlock(s2b1);
        
        // Clear session 1
        storage.clearSession(1);
        
        // Session 1 block should be gone
        auto result1 = storage.getByCommandId(10, 1);
        REQUIRE_FALSE(result1.has_value());
        
        // Session 2 block should still exist
        auto result2 = storage.getByCommandId(20, 2);
        REQUIRE(result2.has_value());
        REQUIRE(result2->command == "pwd");
    }
}
