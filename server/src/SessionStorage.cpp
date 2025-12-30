#include "SessionStorage.h"
#include <chrono>

using namespace sqlite_orm;

SessionStorage::SessionStorage(std::filesystem::path dbPath)
    : dbPath(std::move(dbPath))
    , storage(makeSessionStorage(this->dbPath.string()))
{
}

void SessionStorage::initialize() {
    this->storage.sync_schema();
}

uint64_t SessionStorage::addCommand(uint64_t serverRunId, const std::string& command, const std::string& cwdStart) {
    SessionCommand cmd;
    cmd.serverRunId = serverRunId;
    cmd.command = command;
    cmd.cwdStart = cwdStart;
    cmd.isFinished = false;
    cmd.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
    return this->storage.insert(cmd);
}

void SessionStorage::appendOutput(uint64_t commandId, const std::string& output) {
    this->storage.update_all(
        set(c(&SessionCommand::output) = c(&SessionCommand::output) || output),
        where(c(&SessionCommand::id) == commandId)
    );
}

void SessionStorage::finishCommand(uint64_t commandId, int exitCode, const std::string& cwdEnd) {
    this->storage.update_all(
        set(
            c(&SessionCommand::exitCode) = exitCode,
            c(&SessionCommand::cwdEnd) = cwdEnd,
            c(&SessionCommand::isFinished) = true
        ),
        where(c(&SessionCommand::id) == commandId)
    );
}

std::optional<SessionCommand> SessionStorage::getCommand(uint64_t commandId) {
    return this->storage.get_optional<SessionCommand>(commandId);
}

std::vector<SessionCommand> SessionStorage::getCommandsForRun(uint64_t serverRunId) {
    return this->storage.get_all<SessionCommand>(
        where(c(&SessionCommand::serverRunId) == serverRunId),
        order_by(&SessionCommand::id)
    );
}

std::vector<SessionCommand> SessionStorage::getAllCommands() {
    return this->storage.get_all<SessionCommand>(order_by(&SessionCommand::id));
}
