#include "termihui/client_storage.h"

using namespace sqlite_orm;

ClientStorage::ClientStorage(const std::filesystem::path& dbPath)
    : storage(createClientStorage(dbPath.string()))
{
    storage.sync_schema();
}

// Key-Value API

void ClientStorage::set(const std::string& key, const std::string& value) {
    storage.replace(KeyValue{key, value});
}

std::optional<std::string> ClientStorage::get(const std::string& key) {
    auto result = storage.get_pointer<KeyValue>(key);
    if (result) {
        return result->value;
    }
    return std::nullopt;
}

void ClientStorage::remove(const std::string& key) {
    storage.remove<KeyValue>(key);
}

void ClientStorage::setUInt64(const std::string& key, uint64_t value) {
    set(key, std::to_string(value));
}

std::optional<uint64_t> ClientStorage::getUInt64(const std::string& key) {
    auto str = get(key);
    if (str) {
        try {
            return std::stoull(*str);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

// Command Blocks API

int64_t ClientStorage::insertCommandBlock(const CommandBlock& block) {
    return storage.insert(block);
}

void ClientStorage::appendOutput(int64_t localId, std::string_view text) {
    auto block = storage.get_pointer<CommandBlock>(localId);
    if (block) {
        block->output += text;
        storage.update(*block);
    }
}

void ClientStorage::finishCommand(int64_t localId, int exitCode, std::optional<uint64_t> commandId, const std::string& cwdEnd) {
    auto block = storage.get_pointer<CommandBlock>(localId);
    if (block) {
        block->isFinished = true;
        block->exitCode = exitCode;
        block->commandId = commandId;
        if (!cwdEnd.empty()) {
            block->cwdEnd = cwdEnd;
        }
        storage.update(*block);
    }
}

std::optional<CommandBlock> ClientStorage::getByLocalId(int64_t localId) {
    auto result = storage.get_pointer<CommandBlock>(localId);
    if (result) {
        return *result;
    }
    return std::nullopt;
}

std::optional<CommandBlock> ClientStorage::getByCommandId(uint64_t commandId) {
    auto results = storage.get_all<CommandBlock>(
        where(c(&CommandBlock::commandId) == commandId)
    );
    if (!results.empty()) {
        return results[0];
    }
    return std::nullopt;
}

std::optional<CommandBlock> ClientStorage::getLastBlock(uint64_t sessionId) {
    auto results = storage.get_all<CommandBlock>(
        where(c(&CommandBlock::sessionId) == sessionId),
        order_by(&CommandBlock::localId).desc(),
        limit(1)
    );
    if (!results.empty()) {
        return results[0];
    }
    return std::nullopt;
}

std::optional<CommandBlock> ClientStorage::getUnfinishedBlock(uint64_t sessionId) {
    auto results = storage.get_all<CommandBlock>(
        where(c(&CommandBlock::sessionId) == sessionId && c(&CommandBlock::isFinished) == false),
        order_by(&CommandBlock::localId).desc(),
        limit(1)
    );
    if (!results.empty()) {
        return results[0];
    }
    return std::nullopt;
}

std::vector<CommandBlock> ClientStorage::getBlocksForSession(uint64_t sessionId) {
    return storage.get_all<CommandBlock>(
        where(c(&CommandBlock::sessionId) == sessionId),
        order_by(&CommandBlock::localId).asc()
    );
}

void ClientStorage::clearSession(uint64_t sessionId) {
    storage.remove_all<CommandBlock>(
        where(c(&CommandBlock::sessionId) == sessionId)
    );
}

void ClientStorage::clearAllBlocks() {
    storage.remove_all<CommandBlock>();
}