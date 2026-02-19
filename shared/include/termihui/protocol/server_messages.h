#pragma once

#include <cinttypes>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <termihui/text_style.h>

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
    uint64_t sessionId = 0;
    std::vector<StyledSegment> segments;
    
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
    std::vector<StyledSegment> segments;
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
    uint64_t sessionId = 0;
    std::optional<std::string> cwd;
    
    static constexpr const char* type = "command_start";
};

struct CommandEndMessage {
    uint64_t sessionId = 0;
    int exitCode;
    std::optional<std::string> cwd;
    
    static constexpr const char* type = "command_end";
};

struct PromptStartMessage {
    uint64_t sessionId = 0;
    static constexpr const char* type = "prompt_start";
};

struct PromptEndMessage {
    uint64_t sessionId = 0;
    static constexpr const char* type = "prompt_end";
};

struct CwdUpdateMessage {
    std::string cwd;
    
    static constexpr const char* type = "cwd_update";
};

// ============================================================================
// Interactive mode messages
// ============================================================================

/**
 * Sent when entering interactive mode (alternate screen buffer)
 */
struct InteractiveModeStartMessage {
    size_t rows;
    size_t columns;
    
    static constexpr const char* type = "interactive_mode_start";
};

/**
 * Full screen snapshot (sent after InteractiveModeStart and on reconnect)
 */
struct ScreenSnapshotMessage {
    size_t cursorRow;
    size_t cursorColumn;
    std::vector<std::vector<StyledSegment>> lines;  // lines[row] = segments for that row
    
    static constexpr const char* type = "screen_snapshot";
};

/**
 * Single row update for screen diff
 */
struct ScreenRowUpdate {
    size_t row;
    std::vector<StyledSegment> segments;
};

/**
 * Screen diff (only changed rows)
 */
struct ScreenDiffMessage {
    size_t cursorRow;
    size_t cursorColumn;
    std::vector<ScreenRowUpdate> updates;
    
    static constexpr const char* type = "screen_diff";
};

/**
 * Sent when exiting interactive mode
 */
struct InteractiveModeEndMessage {
    static constexpr const char* type = "interactive_mode_end";
};

/**
 * Block mode active screen update (rows that may be overwritten by \r progress)
 * Uses the same ScreenRowUpdate format as interactive mode diffs.
 */
struct BlockScreenUpdateMessage {
    uint64_t sessionId = 0;
    size_t cursorRow;
    size_t cursorColumn;
    std::vector<ScreenRowUpdate> updates;
    
    static constexpr const char* type = "block_screen_update";
};

// ============================================================================
// AI Chat messages
// ============================================================================

// Streaming chunk from LLM
struct AIChunkMessage {
    uint64_t sessionId;
    std::string content;
    
    static constexpr const char* type = "ai_chunk";
};

// Generation finished
struct AIDoneMessage {
    uint64_t sessionId;
    
    static constexpr const char* type = "ai_done";
};

// AI error
struct AIErrorMessage {
    uint64_t sessionId;
    std::string error;
    
    static constexpr const char* type = "ai_error";
};

// Chat message info for history
struct ChatMessageInfo {
    uint64_t id;
    std::string role;      // "user" or "assistant"
    std::string content;
    int64_t createdAt;
};

// Chat history response
struct ChatHistoryMessage {
    uint64_t sessionId;
    std::vector<ChatMessageInfo> messages;
    
    static constexpr const char* type = "chat_history";
};

// ============================================================================
// LLM Provider messages
// ============================================================================

struct LLMProviderInfo {
    uint64_t id;
    std::string name;
    std::string type;
    std::string url;
    std::string model;
    // Note: apiKey is NOT included in list response for security
    int64_t createdAt;
};

struct LLMProvidersListMessage {
    std::vector<LLMProviderInfo> providers;
    
    static constexpr const char* type = "llm_providers_list";
};

struct LLMProviderAddedMessage {
    uint64_t id;
    
    static constexpr const char* type = "llm_provider_added";
};

struct LLMProviderUpdatedMessage {
    uint64_t id;
    
    static constexpr const char* type = "llm_provider_updated";
};

struct LLMProviderDeletedMessage {
    uint64_t id;
    
    static constexpr const char* type = "llm_provider_deleted";
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
    CwdUpdateMessage,
    InteractiveModeStartMessage,
    ScreenSnapshotMessage,
    ScreenDiffMessage,
    InteractiveModeEndMessage,
    BlockScreenUpdateMessage,
    AIChunkMessage,
    AIDoneMessage,
    AIErrorMessage,
    ChatHistoryMessage,
    LLMProvidersListMessage,
    LLMProviderAddedMessage,
    LLMProviderUpdatedMessage,
    LLMProviderDeletedMessage
>;
