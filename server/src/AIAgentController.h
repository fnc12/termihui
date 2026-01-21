#pragma once

#include <curl/curl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * Events returned by AIAgentController::update()
 */
struct AIEvent {
    enum class Type { 
        Chunk,  // New text chunk from LLM
        Done,   // Generation finished
        Error   // Error occurred
    };
    
    Type type;
    uint64_t sessionId;
    std::string content;  // Text for Chunk, error message for Error
};

/**
 * AI Agent Controller - manages chat sessions and LLM API requests
 * Uses curl_multi for non-blocking streaming requests
 */
class AIAgentController {
public:
    AIAgentController();
    ~AIAgentController();
    
    // Non-copyable
    AIAgentController(const AIAgentController&) = delete;
    AIAgentController& operator=(const AIAgentController&) = delete;
    
    /**
     * Configure the LLM endpoint
     * @param endpoint Base URL (e.g., "http://192.168.68.111:12366")
     */
    void setEndpoint(std::string endpoint);
    
    /**
     * Configure the model name
     * @param model Model identifier (e.g., "/home/eugene/Desktop/models/Qwen3-32B-Q4_K_M.gguf")
     */
    void setModel(std::string model);
    
    /**
     * Configure the API key
     * @param apiKey API key for authentication (optional, can be empty)
     */
    void setApiKey(std::string apiKey);
    
    /**
     * Send a message to the AI for a session
     * Adds message to history and starts streaming request
     * @param sessionId Terminal session ID
     * @param message User message
     */
    void sendMessage(uint64_t sessionId, const std::string& message);
    
    /**
     * Process pending requests - call every tick
     * @return Vector of events (chunks, done, errors)
     */
    std::vector<AIEvent> update();
    
    /**
     * Clear chat history for a session
     * @param sessionId Terminal session ID
     */
    void clearHistory(uint64_t sessionId);
    
private:
    // Chat message structure
    struct ChatMessage {
        std::string role;     // "user" or "assistant"
        std::string content;
    };
    
    // Active streaming request
    struct ActiveRequest {
        CURL* handle = nullptr;
        uint64_t sessionId = 0;
        std::string responseBuffer;   // Accumulated response data
        std::string partialLine;      // Incomplete SSE line
        std::string accumulatedContent; // Full assistant response (for history)
        struct curl_slist* headers = nullptr;
        std::string postData;         // Keep POST data alive
    };
    
    // curl_multi handle
    CURLM* multiHandle = nullptr;
    
    // Configuration
    std::string endpoint;
    std::string model;
    std::string apiKey;
    
    // Chat history per session: sessionId -> messages
    std::unordered_map<uint64_t, std::vector<ChatMessage>> chatHistory;
    
    // Active requests: CURL handle -> request data
    std::unordered_map<CURL*, std::unique_ptr<ActiveRequest>> activeRequests;
    
    // CURL write callback (receives streaming data)
    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata);
    
    // Parse SSE events from buffer and return AI events
    std::vector<AIEvent> parseSSEBuffer(ActiveRequest& req);
    
    // Build JSON request body for chat completion
    std::string buildRequestBody(uint64_t sessionId, const std::string& message);
    
    // Cleanup completed request
    void cleanupRequest(CURL* handle);
};
