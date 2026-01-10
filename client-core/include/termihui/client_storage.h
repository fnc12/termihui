#pragma once

#include <sqlite_orm/sqlite_orm.h>
#include <string>
#include <optional>
#include <vector>
#include <filesystem>

struct KeyValue {
    std::string key;
    std::string value;
};

struct CommandBlock {
    int64_t localId = 0;                    // autoincrement local ID
    std::optional<uint64_t> commandId;      // server-provided ID (null for in-progress)
    uint64_t sessionId = 0;
    std::string command;
    std::string output;
    bool isFinished = false;
    int exitCode = 0;
    std::string cwdStart;
    std::string cwdEnd;
    int64_t timestamp = 0;
};

inline auto createClientStorage(std::string path) {
    using namespace sqlite_orm;
    return make_storage(std::move(path),
        make_table("key_value",
            make_column("key", &KeyValue::key, primary_key()),
            make_column("value", &KeyValue::value)
        ),
        make_table("command_blocks",
            make_column("local_id", &CommandBlock::localId, primary_key().autoincrement()),
            make_column("command_id", &CommandBlock::commandId),
            make_column("session_id", &CommandBlock::sessionId),
            make_column("command", &CommandBlock::command),
            make_column("output", &CommandBlock::output),
            make_column("is_finished", &CommandBlock::isFinished),
            make_column("exit_code", &CommandBlock::exitCode),
            make_column("cwd_start", &CommandBlock::cwdStart),
            make_column("cwd_end", &CommandBlock::cwdEnd),
            make_column("timestamp", &CommandBlock::timestamp)
        )
    );
}

using ClientStorageType = decltype(createClientStorage(""));

class ClientStorage {
public:
    explicit ClientStorage(const std::filesystem::path& dbPath);
    
    // Key-Value API
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    void remove(const std::string& key);
    
    // Convenience methods for common types
    void setUInt64(const std::string& key, uint64_t value);
    std::optional<uint64_t> getUInt64(const std::string& key);
    
    // Command Blocks API
    
    /// Insert new command block (returns localId)
    int64_t insertCommandBlock(const CommandBlock& block);
    
    /// Append text to output of a command block
    void appendOutput(int64_t localId, std::string_view text);
    
    /// Mark command as finished with exit code and server ID
    void finishCommand(int64_t localId, int exitCode, std::optional<uint64_t> commandId, const std::string& cwdEnd = "");
    
    /// Get command block by local ID
    std::optional<CommandBlock> getByLocalId(int64_t localId);
    
    /// Get command block by server command ID
    std::optional<CommandBlock> getByCommandId(uint64_t commandId);
    
    /// Get last (most recent) command block for session
    std::optional<CommandBlock> getLastBlock(uint64_t sessionId);
    
    /// Get unfinished (in-progress) command block for session
    std::optional<CommandBlock> getUnfinishedBlock(uint64_t sessionId);
    
    /// Get all command blocks for session (ordered by localId)
    std::vector<CommandBlock> getBlocksForSession(uint64_t sessionId);
    
    /// Clear all command blocks for session (e.g. when switching sessions)
    void clearSession(uint64_t sessionId);
    
    /// Clear all command blocks
    void clearAllBlocks();

private:
    ClientStorageType storage;
};
