#include "ServerStorage.h"
#include <chrono>

using namespace sqlite_orm;

ServerStorage::ServerStorage(const std::filesystem::path& dbPath)
    : storage(createServerStorage(dbPath.string()))
{
    this->storage.sync_schema();
}

uint64_t ServerStorage::recordStart() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    ServerRun run{0, timestamp};
    auto id = this->storage.insert(run);
    return static_cast<uint64_t>(id);
}

void ServerStorage::recordStop(uint64_t runId) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    ServerStop stop{0, runId, timestamp};
    this->storage.insert(stop);
}

std::optional<ServerRun> ServerStorage::getLastRun() {
    auto runs = this->storage.get_all<ServerRun>(
        order_by(&ServerRun::id).desc(),
        limit(1)
    );
    
    if (runs.empty()) {
        return std::nullopt;
    }
    return runs[0];
}

std::optional<ServerStop> ServerStorage::getStopForRun(uint64_t runId) {
    auto stops = this->storage.get_all<ServerStop>(
        where(c(&ServerStop::runId) == runId)
    );
    
    if (stops.empty()) {
        return std::nullopt;
    }
    return stops[0];
}

bool ServerStorage::wasLastRunCrashed() {
    auto lastRun = this->getLastRun();
    if (!lastRun) {
        return false;
    }
    
    auto stop = this->getStopForRun(lastRun->id);
    return !stop.has_value();
}

uint64_t ServerStorage::createTerminalSession(uint64_t serverRunId) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    TerminalSession session;
    session.serverRunId = serverRunId;
    session.createdAt = timestamp;
    session.isDeleted = false;
    session.deletedAt = 0;
    
    return static_cast<uint64_t>(this->storage.insert(session));
}

void ServerStorage::markTerminalSessionAsDeleted(uint64_t sessionId) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    
    this->storage.update_all(
        set(
            c(&TerminalSession::isDeleted) = true,
            c(&TerminalSession::deletedAt) = timestamp
        ),
        where(c(&TerminalSession::id) == sessionId)
    );
}

std::optional<TerminalSession> ServerStorage::getTerminalSession(uint64_t sessionId) {
    return this->storage.get_optional<TerminalSession>(sessionId);
}

std::vector<TerminalSession> ServerStorage::getActiveTerminalSessions() {
    return this->storage.get_all<TerminalSession>(
        where(c(&TerminalSession::isDeleted) == false),
        order_by(&TerminalSession::id)
    );
}
