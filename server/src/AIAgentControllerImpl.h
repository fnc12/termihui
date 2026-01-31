#pragma once

#include "AIAgentController.h"
#include <curl/curl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * AI Agent Controller implementation using curl_multi for non-blocking streaming requests
 */
class AIAgentControllerImpl : public AIAgentController {
public:
    AIAgentControllerImpl();
    ~AIAgentControllerImpl() override;
    
    // Non-copyable
    AIAgentControllerImpl(const AIAgentControllerImpl&) = delete;
    AIAgentControllerImpl& operator=(const AIAgentControllerImpl&) = delete;
    
    void setEndpoint(std::string endpoint) override;
    void setModel(std::string model) override;
    void setApiKey(std::string apiKey) override;
    void sendMessage(uint64_t sessionId, const std::string& message) override;
    std::vector<AIEvent> update() override;
    void clearHistory(uint64_t sessionId) override;
    
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
        std::string url;              // Keep URL alive (CURL stores pointer only)
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
