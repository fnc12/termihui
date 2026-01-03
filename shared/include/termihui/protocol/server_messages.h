#pragma once

#include <cinttypes>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// ============================================================================
// Server → Client messages
// ============================================================================

struct ConnectedMessage {
    std::string serverVersion;
    std::optional<std::string> home;
    
    static constexpr const char* type = "connected";
};

struct ErrorMessage {
    std::string message;
    std::string errorCode;
    
    static constexpr const char* type = "error";
};

struct OutputMessage {
    std::string data;
    
    static constexpr const char* type = "output";
};

struct StatusMessage {
    uint64_t sessionId;
    bool running;
    
    static constexpr const char* type = "status";
};

struct InputSentMessage {
    int bytes;
    
    static constexpr const char* type = "input_sent";
};

struct CompletionResultMessage {
    std::vector<std::string> completions;
    std::string originalText;
    int cursorPosition;
    
    static constexpr const char* type = "completion_result";
};

struct ResizeAckMessage {
    int cols;
    int rows;
    
    static constexpr const char* type = "resize_ack";
};

struct SessionInfo {
    uint64_t id;
    int64_t createdAt;
};

struct SessionsListMessage {
    std::vector<SessionInfo> sessions;
    
    static constexpr const char* type = "sessions_list";
};

struct SessionCreatedMessage {
    uint64_t sessionId;
    
    static constexpr const char* type = "session_created";
};

struct SessionClosedMessage {
    uint64_t sessionId;
    
    static constexpr const char* type = "session_closed";
};

struct CommandRecord {
    uint64_t id;
    std::string command;
    std::string output;
    int exitCode;
    std::string cwdStart;
    std::string cwdEnd;
    bool isFinished;
};

struct HistoryMessage {
    uint64_t sessionId;
    std::vector<CommandRecord> commands;
    
    static constexpr const char* type = "history";
};

struct CommandStartMessage {
    std::optional<std::string> cwd;
    
    static constexpr const char* type = "command_start";
};

struct CommandEndMessage {
    int exitCode;
    std::optional<std::string> cwd;
    
    static constexpr const char* type = "command_end";
};

struct PromptStartMessage {
    static constexpr const char* type = "prompt_start";
};

struct PromptEndMessage {
    static constexpr const char* type = "prompt_end";
};

struct CwdUpdateMessage {
    std::string cwd;
    
    static constexpr const char* type = "cwd_update";
};

// Variant alias for all server messages
using ServerMessage = std::variant<
    ConnectedMessage,
    ErrorMessage,
    OutputMessage,
    StatusMessage,
    InputSentMessage,
    CompletionResultMessage,
    ResizeAckMessage,
    SessionsListMessage,
    SessionCreatedMessage,
    SessionClosedMessage,
    HistoryMessage,
    CommandStartMessage,
    CommandEndMessage,
    PromptStartMessage,
    PromptEndMessage,
    CwdUpdateMessage
>;
