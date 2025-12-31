#pragma once

#include <sqlite_orm/sqlite_orm.h>
#include <cstdint>
#include <string>

struct ServerRun {
    uint64_t id = 0;
    int64_t startTimestamp = 0;
};

struct ServerStop {
    uint64_t id = 0;
    uint64_t runId = 0;
    int64_t stopTimestamp = 0;
};

struct TerminalSession {
    uint64_t id = 0;
    uint64_t serverRunId = 0;      // Server run when session was created
    int64_t createdAt = 0;
    bool isDeleted = false;
    int64_t deletedAt = 0;         // 0 if not deleted
};

inline auto createServerStorage(std::string path) {
    using namespace sqlite_orm;
    return make_storage(std::move(path),
        make_table("server_runs",
            make_column("id", &ServerRun::id, primary_key().autoincrement()),
            make_column("start_timestamp", &ServerRun::startTimestamp)
        ),
        make_table("server_stops",
            make_column("id", &ServerStop::id, primary_key().autoincrement()),
            make_column("run_id", &ServerStop::runId),
            make_column("stop_timestamp", &ServerStop::stopTimestamp)
        ),
        make_table("terminal_sessions",
            make_column("id", &TerminalSession::id, primary_key().autoincrement()),
            make_column("server_run_id", &TerminalSession::serverRunId),
            make_column("created_at", &TerminalSession::createdAt),
            make_column("is_deleted", &TerminalSession::isDeleted),
            make_column("deleted_at", &TerminalSession::deletedAt)
        )
    );
}

using ServerStorageType = decltype(createServerStorage(""));

