#pragma once

#include <cstdint>
#include <string>
#include <variant>

// ============================================================================
// Client → Server messages
// ============================================================================

struct ExecuteMessage {
    uint64_t sessionId;
    std::string command;
    
    static constexpr const char* type = "execute";
};

struct InputMessage {
    uint64_t sessionId;
    std::string text;
    
    static constexpr const char* type = "input";
};

struct CompletionMessage {
    uint64_t sessionId;
    std::string text;
    int cursorPosition;
    
    static constexpr const char* type = "completion";
};

struct ResizeMessage {
    uint64_t sessionId;
    int cols;
    int rows;
    
    static constexpr const char* type = "resize";
};

struct ListSessionsMessage {
    static constexpr const char* type = "list_sessions";
};

struct CreateSessionMessage {
    static constexpr const char* type = "create_session";
};

struct CloseSessionMessage {
    uint64_t sessionId;
    
    static constexpr const char* type = "close_session";
};

struct GetHistoryMessage {
    uint64_t sessionId;
    
    static constexpr const char* type = "get_history";
};

// Variant alias for all client messages
using ClientMessage = std::variant<
    ExecuteMessage,
    InputMessage,
    CompletionMessage,
    ResizeMessage,
    ListSessionsMessage,
    CreateSessionMessage,
    CloseSessionMessage,
    GetHistoryMessage
>;
