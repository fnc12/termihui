#pragma once

#include <string>
#include <sqlite_orm/sqlite_orm.h>
#include "SessionStorageModels.h"

inline auto makeSessionStorage(std::string path) {
    using namespace sqlite_orm;
    return make_storage(std::move(path),
        make_table("session_commands",
            make_column("id", &SessionCommand::id, primary_key().autoincrement()),
            make_column("server_run_id", &SessionCommand::serverRunId),
            make_column("command", &SessionCommand::command),
            make_column("output", &SessionCommand::output),
            make_column("exit_code", &SessionCommand::exitCode),
            make_column("cwd_start", &SessionCommand::cwdStart),
            make_column("cwd_end", &SessionCommand::cwdEnd),
            make_column("is_finished", &SessionCommand::isFinished),
            make_column("timestamp", &SessionCommand::timestamp)
        ),
        make_table("command_output_lines",
            make_column("id", &CommandOutputLine::id, primary_key().autoincrement()),
            make_column("command_id", &CommandOutputLine::commandId),
            make_column("line_order", &CommandOutputLine::lineOrder),
            make_column("segments_json", &CommandOutputLine::segmentsJson)
        )
    );
}

using SessionStorageType = decltype(makeSessionStorage(""));

