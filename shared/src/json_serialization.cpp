#include <termihui/protocol/json_serialization.h>
#include <stdexcept>

// ============================================================================
// Client Messages JSON serialization
// ============================================================================

void to_json(json& j, const ExecuteMessage& message) {
    j = json{
        {"type", ExecuteMessage::type},
        {"session_id", message.sessionId},
        {"command", message.command}
    };
}

void from_json(const json& j, ExecuteMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("command").get_to(message.command);
}

void to_json(json& j, const InputMessage& message) {
    j = json{
        {"type", InputMessage::type},
        {"session_id", message.sessionId},
        {"text", message.text}
    };
}

void from_json(const json& j, InputMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("text").get_to(message.text);
}

void to_json(json& j, const CompletionMessage& message) {
    j = json{
        {"type", CompletionMessage::type},
        {"session_id", message.sessionId},
        {"text", message.text},
        {"cursor_position", message.cursorPosition}
    };
}

void from_json(const json& j, CompletionMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("text").get_to(message.text);
    j.at("cursor_position").get_to(message.cursorPosition);
}

void to_json(json& j, const ResizeMessage& message) {
    j = json{
        {"type", ResizeMessage::type},
        {"session_id", message.sessionId},
        {"cols", message.cols},
        {"rows", message.rows}
    };
}

void from_json(const json& j, ResizeMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("cols").get_to(message.cols);
    j.at("rows").get_to(message.rows);
}

void to_json(json& j, const ListSessionsMessage&) {
    j = json{{"type", ListSessionsMessage::type}};
}

void from_json(const json&, ListSessionsMessage&) {
    // No fields to parse
}

void to_json(json& j, const CreateSessionMessage&) {
    j = json{{"type", CreateSessionMessage::type}};
}

void from_json(const json&, CreateSessionMessage&) {
    // No fields to parse
}

void to_json(json& j, const CloseSessionMessage& message) {
    j = json{
        {"type", CloseSessionMessage::type},
        {"session_id", message.sessionId}
    };
}

void from_json(const json& j, CloseSessionMessage& message) {
    j.at("session_id").get_to(message.sessionId);
}

void to_json(json& j, const GetHistoryMessage& message) {
    j = json{
        {"type", GetHistoryMessage::type},
        {"session_id", message.sessionId}
    };
}

void from_json(const json& j, GetHistoryMessage& message) {
    j.at("session_id").get_to(message.sessionId);
}

// ClientMessage variant serialization

void to_json(json& j, const ClientMessage& message) {
    std::visit([&j](const auto& m) { to_json(j, m); }, message);
}

void from_json(const json& j, ClientMessage& message) {
    std::string type = j.at("type").get<std::string>();
    
    if (type == ExecuteMessage::type) {
        ExecuteMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == InputMessage::type) {
        InputMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == CompletionMessage::type) {
        CompletionMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == ResizeMessage::type) {
        ResizeMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == ListSessionsMessage::type) {
        message = ListSessionsMessage{};
    } else if (type == CreateSessionMessage::type) {
        message = CreateSessionMessage{};
    } else if (type == CloseSessionMessage::type) {
        CloseSessionMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == GetHistoryMessage::type) {
        GetHistoryMessage m;
        from_json(j, m);
        message = std::move(m);
    } else {
        throw std::runtime_error("Unknown client message type: " + type);
    }
}

// ============================================================================
// Server Messages JSON serialization
// ============================================================================

void to_json(json& j, const ConnectedMessage& message) {
    j = json{
        {"type", ConnectedMessage::type},
        {"server_version", message.serverVersion}
    };
    if (message.home) {
        j["home"] = *message.home;
    }
}

void from_json(const json& j, ConnectedMessage& message) {
    j.at("server_version").get_to(message.serverVersion);
    if (auto it = j.find("home"); it != j.end()) {
        message.home = it->get<std::string>();
    }
}

void to_json(json& j, const ErrorMessage& message) {
    j = json{
        {"type", ErrorMessage::type},
        {"message", message.message},
        {"error_code", message.errorCode}
    };
}

void from_json(const json& j, ErrorMessage& message) {
    j.at("message").get_to(message.message);
    if (auto it = j.find("error_code"); it != j.end()) {
        it->get_to(message.errorCode);
    }
}

void to_json(json& j, const OutputMessage& message) {
    j = json{
        {"type", OutputMessage::type},
        {"data", message.data}
    };
}

void from_json(const json& j, OutputMessage& message) {
    j.at("data").get_to(message.data);
}

void to_json(json& j, const StatusMessage& message) {
    j = json{
        {"type", StatusMessage::type},
        {"session_id", message.sessionId},
        {"running", message.running}
    };
}

void from_json(const json& j, StatusMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("running").get_to(message.running);
}

void to_json(json& j, const InputSentMessage& message) {
    j = json{
        {"type", InputSentMessage::type},
        {"bytes", message.bytes}
    };
}

void from_json(const json& j, InputSentMessage& message) {
    j.at("bytes").get_to(message.bytes);
}

void to_json(json& j, const CompletionResultMessage& message) {
    j = json{
        {"type", CompletionResultMessage::type},
        {"completions", message.completions},
        {"original_text", message.originalText},
        {"cursor_position", message.cursorPosition}
    };
}

void from_json(const json& j, CompletionResultMessage& message) {
    j.at("completions").get_to(message.completions);
    j.at("original_text").get_to(message.originalText);
    j.at("cursor_position").get_to(message.cursorPosition);
}

void to_json(json& j, const ResizeAckMessage& message) {
    j = json{
        {"type", ResizeAckMessage::type},
        {"cols", message.cols},
        {"rows", message.rows}
    };
}

void from_json(const json& j, ResizeAckMessage& message) {
    j.at("cols").get_to(message.cols);
    j.at("rows").get_to(message.rows);
}

void to_json(json& j, const SessionInfo& info) {
    j = json{
        {"id", info.id},
        {"created_at", info.createdAt}
    };
}

void from_json(const json& j, SessionInfo& info) {
    j.at("id").get_to(info.id);
    info.createdAt = j.at("created_at").get<int64_t>();
}

void to_json(json& j, const SessionsListMessage& message) {
    j = json{
        {"type", SessionsListMessage::type},
        {"sessions", message.sessions}
    };
}

void from_json(const json& j, SessionsListMessage& message) {
    j.at("sessions").get_to(message.sessions);
}

void to_json(json& j, const SessionCreatedMessage& message) {
    j = json{
        {"type", SessionCreatedMessage::type},
        {"session_id", message.sessionId}
    };
}

void from_json(const json& j, SessionCreatedMessage& message) {
    j.at("session_id").get_to(message.sessionId);
}

void to_json(json& j, const SessionClosedMessage& message) {
    j = json{
        {"type", SessionClosedMessage::type},
        {"session_id", message.sessionId}
    };
}

void from_json(const json& j, SessionClosedMessage& message) {
    j.at("session_id").get_to(message.sessionId);
}

void to_json(json& j, const CommandRecord& record) {
    j = json{
        {"id", record.id},
        {"command", record.command},
        {"output", record.output},
        {"exit_code", record.exitCode},
        {"cwd_start", record.cwdStart},
        {"cwd_end", record.cwdEnd},
        {"is_finished", record.isFinished}
    };
}

void from_json(const json& j, CommandRecord& record) {
    j.at("id").get_to(record.id);
    j.at("command").get_to(record.command);
    j.at("output").get_to(record.output);
    j.at("exit_code").get_to(record.exitCode);
    j.at("cwd_start").get_to(record.cwdStart);
    j.at("cwd_end").get_to(record.cwdEnd);
    j.at("is_finished").get_to(record.isFinished);
}

void to_json(json& j, const HistoryMessage& message) {
    j = json{
        {"type", HistoryMessage::type},
        {"session_id", message.sessionId},
        {"commands", message.commands}
    };
}

void from_json(const json& j, HistoryMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("commands").get_to(message.commands);
}

void to_json(json& j, const CommandStartMessage& message) {
    j = json{{"type", CommandStartMessage::type}};
    if (message.cwd) {
        j["cwd"] = *message.cwd;
    }
}

void from_json(const json& j, CommandStartMessage& message) {
    if (auto it = j.find("cwd"); it != j.end()) {
        message.cwd = it->get<std::string>();
    }
}

void to_json(json& j, const CommandEndMessage& message) {
    j = json{
        {"type", CommandEndMessage::type},
        {"exit_code", message.exitCode}
    };
    if (message.cwd) {
        j["cwd"] = *message.cwd;
    }
}

void from_json(const json& j, CommandEndMessage& message) {
    j.at("exit_code").get_to(message.exitCode);
    if (auto it = j.find("cwd"); it != j.end()) {
        message.cwd = it->get<std::string>();
    }
}

void to_json(json& j, const PromptStartMessage&) {
    j = json{{"type", PromptStartMessage::type}};
}

void from_json(const json&, PromptStartMessage&) {
    // No fields
}

void to_json(json& j, const PromptEndMessage&) {
    j = json{{"type", PromptEndMessage::type}};
}

void from_json(const json&, PromptEndMessage&) {
    // No fields
}

void to_json(json& j, const CwdUpdateMessage& message) {
    j = json{
        {"type", CwdUpdateMessage::type},
        {"cwd", message.cwd}
    };
}

void from_json(const json& j, CwdUpdateMessage& message) {
    j.at("cwd").get_to(message.cwd);
}

// ServerMessage variant serialization

void to_json(json& j, const ServerMessage& message) {
    std::visit([&j](const auto& m) { to_json(j, m); }, message);
}

void from_json(const json& j, ServerMessage& message) {
    std::string type = j.at("type").get<std::string>();
    
    if (type == ConnectedMessage::type) {
        ConnectedMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == ErrorMessage::type) {
        ErrorMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == OutputMessage::type) {
        OutputMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == StatusMessage::type) {
        StatusMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == InputSentMessage::type) {
        InputSentMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == CompletionResultMessage::type) {
        CompletionResultMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == ResizeAckMessage::type) {
        ResizeAckMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == SessionsListMessage::type) {
        SessionsListMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == SessionCreatedMessage::type) {
        SessionCreatedMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == SessionClosedMessage::type) {
        SessionClosedMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == HistoryMessage::type) {
        HistoryMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == CommandStartMessage::type) {
        CommandStartMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == CommandEndMessage::type) {
        CommandEndMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == PromptStartMessage::type) {
        message = PromptStartMessage{};
    } else if (type == PromptEndMessage::type) {
        message = PromptEndMessage{};
    } else if (type == CwdUpdateMessage::type) {
        CwdUpdateMessage m;
        from_json(j, m);
        message = std::move(m);
    } else {
        throw std::runtime_error("Unknown server message type: " + type);
    }
}

// ============================================================================
// Helper functions
// ============================================================================

template<typename T>
static std::string serializeImpl(const T& message) {
    json j;
    to_json(j, message);
    return j.dump();
}

std::string serialize(const ClientMessage& message) { return serializeImpl(message); }
std::string serialize(const ServerMessage& message) { return serializeImpl(message); }
std::string serialize(const ExecuteMessage& message) { return serializeImpl(message); }
std::string serialize(const InputMessage& message) { return serializeImpl(message); }
std::string serialize(const CompletionMessage& message) { return serializeImpl(message); }
std::string serialize(const ResizeMessage& message) { return serializeImpl(message); }
std::string serialize(const ListSessionsMessage& message) { return serializeImpl(message); }
std::string serialize(const CreateSessionMessage& message) { return serializeImpl(message); }
std::string serialize(const CloseSessionMessage& message) { return serializeImpl(message); }
std::string serialize(const GetHistoryMessage& message) { return serializeImpl(message); }
std::string serialize(const ConnectedMessage& message) { return serializeImpl(message); }
std::string serialize(const ErrorMessage& message) { return serializeImpl(message); }
std::string serialize(const OutputMessage& message) { return serializeImpl(message); }
std::string serialize(const StatusMessage& message) { return serializeImpl(message); }
std::string serialize(const InputSentMessage& message) { return serializeImpl(message); }
std::string serialize(const CompletionResultMessage& message) { return serializeImpl(message); }
std::string serialize(const ResizeAckMessage& message) { return serializeImpl(message); }
std::string serialize(const SessionsListMessage& message) { return serializeImpl(message); }
std::string serialize(const SessionCreatedMessage& message) { return serializeImpl(message); }
std::string serialize(const SessionClosedMessage& message) { return serializeImpl(message); }
std::string serialize(const HistoryMessage& message) { return serializeImpl(message); }
std::string serialize(const CommandStartMessage& message) { return serializeImpl(message); }
std::string serialize(const CommandEndMessage& message) { return serializeImpl(message); }
std::string serialize(const PromptStartMessage& message) { return serializeImpl(message); }
std::string serialize(const PromptEndMessage& message) { return serializeImpl(message); }
std::string serialize(const CwdUpdateMessage& message) { return serializeImpl(message); }

ClientMessage parseClientMessage(const std::string& jsonStr) {
    json j = json::parse(jsonStr);
    ClientMessage message;
    from_json(j, message);
    return message;
}

ServerMessage parseServerMessage(const std::string& jsonStr) {
    json j = json::parse(jsonStr);
    ServerMessage message;
    from_json(j, message);
    return message;
}
