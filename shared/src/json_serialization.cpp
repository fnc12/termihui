#include <termihui/protocol/json_serialization.h>
#include <stdexcept>
#include <array>
#include <fmt/core.h>

// ============================================================================
// Style types JSON serialization
// ============================================================================

void to_json(json& j, const Color& color) {
    switch (color.type) {
        case Color::Type::Standard:
            j = std::string(colorName(color.index));
            break;
        case Color::Type::Bright:
            j = std::string("bright_") + std::string(colorName(color.index));
            break;
        case Color::Type::Indexed:
            j = json{{"index", color.index}};
            break;
        case Color::Type::RGB:
            j = json{{"rgb", fmt::format("#{:02X}{:02X}{:02X}", color.r, color.g, color.b)}};
            break;
    }
}

void from_json(const json& j, Color& color) {
    if (j.is_string()) {
        std::string name = j.get<std::string>();
        static constexpr std::array<std::string_view, 8> colorNames = {
            "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"
        };
        
        // Check for bright_ prefix
        if (name.rfind("bright_", 0) == 0) {
            std::string_view baseName{name.data() + 7, name.size() - 7};
            for (size_t i = 0; i < colorNames.size(); ++i) {
                if (baseName == colorNames[i]) {
                    color = Color::bright(static_cast<int>(i));
                    return;
                }
            }
        } else {
            for (size_t i = 0; i < colorNames.size(); ++i) {
                if (name == colorNames[i]) {
                    color = Color::standard(static_cast<int>(i));
                    return;
                }
            }
        }
    } else if (j.is_object()) {
        if (auto it = j.find("index"); it != j.end()) {
            color = Color::indexed(it->get<int>());
        } else if (auto it = j.find("rgb"); it != j.end()) {
            std::string rgb = it->get<std::string>();
            // Parse #RRGGBB
            if (rgb.length() == 7 && rgb[0] == '#') {
                int r = std::stoi(rgb.substr(1, 2), nullptr, 16);
                int g = std::stoi(rgb.substr(3, 2), nullptr, 16);
                int b = std::stoi(rgb.substr(5, 2), nullptr, 16);
                color = Color::rgb(r, g, b);
            }
        }
    }
}

void to_json(json& j, const TextStyle& style) {
    j["fg"] = style.foreground ? json(*style.foreground) : json(nullptr);
    j["bg"] = style.background ? json(*style.background) : json(nullptr);
    j["bold"] = style.bold;
    j["dim"] = style.dim;
    j["italic"] = style.italic;
    j["underline"] = style.underline;
    j["reverse"] = style.reverse;
    j["strikethrough"] = style.strikethrough;
}

void from_json(const json& j, TextStyle& style) {
    style.reset();
    
    if (auto it = j.find("fg"); it != j.end() && !it->is_null()) {
        style.foreground = it->get<Color>();
    }
    if (auto it = j.find("bg"); it != j.end() && !it->is_null()) {
        style.background = it->get<Color>();
    }
    if (auto it = j.find("bold"); it != j.end()) it->get_to(style.bold);
    if (auto it = j.find("dim"); it != j.end()) it->get_to(style.dim);
    if (auto it = j.find("italic"); it != j.end()) it->get_to(style.italic);
    if (auto it = j.find("underline"); it != j.end()) it->get_to(style.underline);
    if (auto it = j.find("reverse"); it != j.end()) it->get_to(style.reverse);
    if (auto it = j.find("strikethrough"); it != j.end()) it->get_to(style.strikethrough);
}

void to_json(json& j, const StyledSegment& segment) {
    j["text"] = segment.text;
    j["style"] = segment.style;
}

void from_json(const json& j, StyledSegment& segment) {
    j.at("text").get_to(segment.text);
    j.at("style").get_to(segment.style);
}

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

void to_json(json& j, const AIChatMessage& message) {
    j = json{
        {"type", AIChatMessage::type},
        {"session_id", message.sessionId},
        {"provider_id", message.providerId},
        {"message", message.message}
    };
}

void from_json(const json& j, AIChatMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("provider_id").get_to(message.providerId);
    j.at("message").get_to(message.message);
}

// LLM Provider client messages

void to_json(json& j, const ListLLMProvidersMessage&) {
    j = json{{"type", ListLLMProvidersMessage::type}};
}

void from_json(const json&, ListLLMProvidersMessage&) {
    // No fields
}

void to_json(json& j, const AddLLMProviderMessage& message) {
    j = json{
        {"type", AddLLMProviderMessage::type},
        {"name", message.name},
        {"provider_type", message.providerType},
        {"url", message.url},
        {"model", message.model},
        {"api_key", message.apiKey}
    };
}

void from_json(const json& j, AddLLMProviderMessage& message) {
    j.at("name").get_to(message.name);
    j.at("provider_type").get_to(message.providerType);
    j.at("url").get_to(message.url);
    if (auto it = j.find("model"); it != j.end()) {
        it->get_to(message.model);
    }
    if (auto it = j.find("api_key"); it != j.end()) {
        it->get_to(message.apiKey);
    }
}

void to_json(json& j, const UpdateLLMProviderMessage& message) {
    j = json{
        {"type", UpdateLLMProviderMessage::type},
        {"id", message.id},
        {"name", message.name},
        {"url", message.url},
        {"model", message.model},
        {"api_key", message.apiKey}
    };
}

void from_json(const json& j, UpdateLLMProviderMessage& message) {
    j.at("id").get_to(message.id);
    j.at("name").get_to(message.name);
    j.at("url").get_to(message.url);
    if (auto it = j.find("model"); it != j.end()) {
        it->get_to(message.model);
    }
    if (auto it = j.find("api_key"); it != j.end()) {
        it->get_to(message.apiKey);
    }
}

void to_json(json& j, const DeleteLLMProviderMessage& message) {
    j = json{
        {"type", DeleteLLMProviderMessage::type},
        {"id", message.id}
    };
}

void from_json(const json& j, DeleteLLMProviderMessage& message) {
    j.at("id").get_to(message.id);
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
    } else if (type == AIChatMessage::type) {
        AIChatMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == ListLLMProvidersMessage::type) {
        message = ListLLMProvidersMessage{};
    } else if (type == AddLLMProviderMessage::type) {
        AddLLMProviderMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == UpdateLLMProviderMessage::type) {
        UpdateLLMProviderMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == DeleteLLMProviderMessage::type) {
        DeleteLLMProviderMessage m;
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
    j["type"] = OutputMessage::type;
    j["segments"] = message.segments;
}

void from_json(const json& j, OutputMessage& message) {
    j.at("segments").get_to(message.segments);
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
        {"segments", record.segments},
        {"exit_code", record.exitCode},
        {"cwd_start", record.cwdStart},
        {"cwd_end", record.cwdEnd},
        {"is_finished", record.isFinished}
    };
}

void from_json(const json& j, CommandRecord& record) {
    j.at("id").get_to(record.id);
    j.at("command").get_to(record.command);
    if (auto it = j.find("segments"); it != j.end()) {
        it->get_to(record.segments);
    }
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

// ============================================================================
// Interactive mode messages
// ============================================================================

void to_json(json& j, const InteractiveModeStartMessage& message) {
    j = json{
        {"type", InteractiveModeStartMessage::type},
        {"rows", message.rows},
        {"columns", message.columns}
    };
}

void from_json(const json& j, InteractiveModeStartMessage& message) {
    j.at("rows").get_to(message.rows);
    j.at("columns").get_to(message.columns);
}

void to_json(json& j, const ScreenSnapshotMessage& message) {
    j = json{
        {"type", ScreenSnapshotMessage::type},
        {"cursor_row", message.cursorRow},
        {"cursor_column", message.cursorColumn},
        {"lines", message.lines}
    };
}

void from_json(const json& j, ScreenSnapshotMessage& message) {
    j.at("cursor_row").get_to(message.cursorRow);
    j.at("cursor_column").get_to(message.cursorColumn);
    j.at("lines").get_to(message.lines);
}

void to_json(json& j, const ScreenRowUpdate& update) {
    j = json{
        {"row", update.row},
        {"segments", update.segments}
    };
}

void from_json(const json& j, ScreenRowUpdate& update) {
    j.at("row").get_to(update.row);
    j.at("segments").get_to(update.segments);
}

void to_json(json& j, const ScreenDiffMessage& message) {
    j = json{
        {"type", ScreenDiffMessage::type},
        {"cursor_row", message.cursorRow},
        {"cursor_column", message.cursorColumn},
        {"updates", message.updates}
    };
}

void from_json(const json& j, ScreenDiffMessage& message) {
    j.at("cursor_row").get_to(message.cursorRow);
    j.at("cursor_column").get_to(message.cursorColumn);
    j.at("updates").get_to(message.updates);
}

void to_json(json& j, const InteractiveModeEndMessage&) {
    j = json{{"type", InteractiveModeEndMessage::type}};
}

void from_json(const json&, InteractiveModeEndMessage&) {
    // No fields
}

void to_json(json& j, const AIChunkMessage& message) {
    j = json{
        {"type", AIChunkMessage::type},
        {"session_id", message.sessionId},
        {"content", message.content}
    };
}

void from_json(const json& j, AIChunkMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("content").get_to(message.content);
}

void to_json(json& j, const AIDoneMessage& message) {
    j = json{
        {"type", AIDoneMessage::type},
        {"session_id", message.sessionId}
    };
}

void from_json(const json& j, AIDoneMessage& message) {
    j.at("session_id").get_to(message.sessionId);
}

void to_json(json& j, const AIErrorMessage& message) {
    j = json{
        {"type", AIErrorMessage::type},
        {"session_id", message.sessionId},
        {"error", message.error}
    };
}

void from_json(const json& j, AIErrorMessage& message) {
    j.at("session_id").get_to(message.sessionId);
    j.at("error").get_to(message.error);
}

// LLM Provider server messages

void to_json(json& j, const LLMProviderInfo& info) {
    j = json{
        {"id", info.id},
        {"name", info.name},
        {"type", info.type},
        {"url", info.url},
        {"model", info.model},
        {"created_at", info.createdAt}
    };
}

void from_json(const json& j, LLMProviderInfo& info) {
    j.at("id").get_to(info.id);
    j.at("name").get_to(info.name);
    j.at("type").get_to(info.type);
    j.at("url").get_to(info.url);
    if (auto it = j.find("model"); it != j.end()) {
        it->get_to(info.model);
    }
    info.createdAt = j.at("created_at").get<int64_t>();
}

void to_json(json& j, const LLMProvidersListMessage& message) {
    j = json{
        {"type", LLMProvidersListMessage::type},
        {"providers", message.providers}
    };
}

void from_json(const json& j, LLMProvidersListMessage& message) {
    j.at("providers").get_to(message.providers);
}

void to_json(json& j, const LLMProviderAddedMessage& message) {
    j = json{
        {"type", LLMProviderAddedMessage::type},
        {"id", message.id}
    };
}

void from_json(const json& j, LLMProviderAddedMessage& message) {
    j.at("id").get_to(message.id);
}

void to_json(json& j, const LLMProviderUpdatedMessage& message) {
    j = json{
        {"type", LLMProviderUpdatedMessage::type},
        {"id", message.id}
    };
}

void from_json(const json& j, LLMProviderUpdatedMessage& message) {
    j.at("id").get_to(message.id);
}

void to_json(json& j, const LLMProviderDeletedMessage& message) {
    j = json{
        {"type", LLMProviderDeletedMessage::type},
        {"id", message.id}
    };
}

void from_json(const json& j, LLMProviderDeletedMessage& message) {
    j.at("id").get_to(message.id);
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
    } else if (type == AIChunkMessage::type) {
        AIChunkMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == AIDoneMessage::type) {
        AIDoneMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == AIErrorMessage::type) {
        AIErrorMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == LLMProvidersListMessage::type) {
        LLMProvidersListMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == LLMProviderAddedMessage::type) {
        LLMProviderAddedMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == LLMProviderUpdatedMessage::type) {
        LLMProviderUpdatedMessage m;
        from_json(j, m);
        message = std::move(m);
    } else if (type == LLMProviderDeletedMessage::type) {
        LLMProviderDeletedMessage m;
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
std::string serialize(const InteractiveModeStartMessage& message) { return serializeImpl(message); }
std::string serialize(const ScreenSnapshotMessage& message) { return serializeImpl(message); }
std::string serialize(const ScreenDiffMessage& message) { return serializeImpl(message); }
std::string serialize(const InteractiveModeEndMessage& message) { return serializeImpl(message); }
std::string serialize(const AIChatMessage& message) { return serializeImpl(message); }
std::string serialize(const AIChunkMessage& message) { return serializeImpl(message); }
std::string serialize(const AIDoneMessage& message) { return serializeImpl(message); }
std::string serialize(const AIErrorMessage& message) { return serializeImpl(message); }
std::string serialize(const ListLLMProvidersMessage& message) { return serializeImpl(message); }
std::string serialize(const AddLLMProviderMessage& message) { return serializeImpl(message); }
std::string serialize(const UpdateLLMProviderMessage& message) { return serializeImpl(message); }
std::string serialize(const DeleteLLMProviderMessage& message) { return serializeImpl(message); }
std::string serialize(const LLMProvidersListMessage& message) { return serializeImpl(message); }
std::string serialize(const LLMProviderAddedMessage& message) { return serializeImpl(message); }
std::string serialize(const LLMProviderUpdatedMessage& message) { return serializeImpl(message); }
std::string serialize(const LLMProviderDeletedMessage& message) { return serializeImpl(message); }

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
