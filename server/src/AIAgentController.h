#pragma once

#include <string>
#include <vector>
#include <cstdint>

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
 * AI Agent Controller interface - manages chat sessions and LLM API requests
 */
class AIAgentController {
public:
    /**
     * Virtual destructor
     */
    virtual ~AIAgentController() = default;
    
    /**
     * Configure the LLM endpoint
     * @param endpoint Base URL (e.g., "http://192.168.68.111:12366")
     */
    virtual void setEndpoint(std::string endpoint) = 0;
    
    /**
     * Configure the model name
     * @param model Model identifier (e.g., "/home/eugene/Desktop/models/Qwen3-32B-Q4_K_M.gguf")
     */
    virtual void setModel(std::string model) = 0;
    
    /**
     * Configure the API key
     * @param apiKey API key for authentication (optional, can be empty)
     */
    virtual void setApiKey(std::string apiKey) = 0;
    
    /**
     * Send a message to the AI for a session
     * Adds message to history and starts streaming request
     * @param sessionId Terminal session ID
     * @param message User message
     */
    virtual void sendMessage(uint64_t sessionId, const std::string& message) = 0;
    
    /**
     * Process pending requests - call every tick
     * @return Vector of events (chunks, done, errors)
     */
    virtual std::vector<AIEvent> update() = 0;
    
    /**
     * Clear chat history for a session
     * @param sessionId Terminal session ID
     */
    virtual void clearHistory(uint64_t sessionId) = 0;
};
