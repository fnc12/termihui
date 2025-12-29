#pragma once

#include <sqlite_orm/sqlite_orm.h>
#include <cstdint>
#include <string>

struct ServerRun {
    uint64_t id;
    int64_t startTimestamp;
};

struct ServerStop {
    uint64_t id;
    uint64_t runId;
    int64_t stopTimestamp;
};

inline auto createServerStorage(const std::string& path) {
    using namespace sqlite_orm;
    return make_storage(path,
        make_table("server_runs",
            make_column("id", &ServerRun::id, primary_key().autoincrement()),
            make_column("start_timestamp", &ServerRun::startTimestamp)
        ),
        make_table("server_stops",
            make_column("id", &ServerStop::id, primary_key().autoincrement()),
            make_column("run_id", &ServerStop::runId),
            make_column("stop_timestamp", &ServerStop::stopTimestamp)
        )
    );
}

using ServerStorageType = decltype(createServerStorage(""));

