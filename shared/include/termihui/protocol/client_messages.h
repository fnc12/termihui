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

// ============================================================================
// AI Chat messages
// ============================================================================

struct AIChatMessage {
    uint64_t sessionId;
    uint64_t providerId;  // Which LLM provider to use
    std::string message;
    
    static constexpr const char* type = "ai_chat";
};

// ============================================================================
// LLM Provider management
// ============================================================================

struct ListLLMProvidersMessage {
    static constexpr const char* type = "list_llm_providers";
};

struct AddLLMProviderMessage {
    std::string name;
    std::string providerType;  // "openai_compatible"
    std::string url;
    std::string model;         // optional
    std::string apiKey;        // optional
    
    static constexpr const char* type = "add_llm_provider";
};

struct UpdateLLMProviderMessage {
    uint64_t id;
    std::string name;
    std::string url;
    std::string model;
    std::string apiKey;
    
    static constexpr const char* type = "update_llm_provider";
};

struct DeleteLLMProviderMessage {
    uint64_t id;
    
    static constexpr const char* type = "delete_llm_provider";
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
    GetHistoryMessage,
    AIChatMessage,
    ListLLMProvidersMessage,
    AddLLMProviderMessage,
    UpdateLLMProviderMessage,
    DeleteLLMProviderMessage
>;
