#pragma once

#include "ServerStorage.h"
#include <variant>
#include <vector>
#include <string>
#include <ostream>

/**
 * Mock ServerStorage for unit tests
 * Records all calls for verification
 */
class ServerStorageMock : public ServerStorage {
public:
    // Call record structures
    struct RecordStartCall {
        bool operator==(const RecordStartCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const RecordStartCall&) {
            return os << "RecordStartCall{}";
        }
    };
    
    struct RecordStopCall {
        uint64_t runId = 0;
        bool operator==(const RecordStopCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const RecordStopCall& c) {
            return os << "RecordStopCall{runId=" << c.runId << "}";
        }
    };
    
    struct SaveChatMessageCall {
        uint64_t sessionId = 0;
        std::string role;
        std::string content;
        bool operator==(const SaveChatMessageCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const SaveChatMessageCall& c) {
            return os << "SaveChatMessageCall{sessionId=" << c.sessionId 
                      << ", role=" << c.role << ", content=" << c.content << "}";
        }
    };
    
    struct GetChatHistoryCall {
        uint64_t sessionId = 0;
        bool operator==(const GetChatHistoryCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const GetChatHistoryCall& c) {
            return os << "GetChatHistoryCall{sessionId=" << c.sessionId << "}";
        }
    };
    
    struct ClearChatHistoryCall {
        uint64_t sessionId = 0;
        bool operator==(const ClearChatHistoryCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const ClearChatHistoryCall& c) {
            return os << "ClearChatHistoryCall{sessionId=" << c.sessionId << "}";
        }
    };
    
    struct CreateTerminalSessionCall {
        uint64_t serverRunId = 0;
        bool operator==(const CreateTerminalSessionCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const CreateTerminalSessionCall& c) {
            return os << "CreateTerminalSessionCall{serverRunId=" << c.serverRunId << "}";
        }
    };
    
    struct IsActiveTerminalSessionCall {
        uint64_t sessionId = 0;
        bool operator==(const IsActiveTerminalSessionCall&) const = default;
        friend std::ostream& operator<<(std::ostream& os, const IsActiveTerminalSessionCall& c) {
            return os << "IsActiveTerminalSessionCall{sessionId=" << c.sessionId << "}";
        }
    };

    using Call = std::variant<
        RecordStartCall, 
        RecordStopCall, 
        SaveChatMessageCall, 
        GetChatHistoryCall,
        ClearChatHistoryCall,
        CreateTerminalSessionCall,
        IsActiveTerminalSessionCall
    >;
    
    friend std::ostream& operator<<(std::ostream& os, const Call& call) {
        std::visit([&os](const auto& c) { os << c; }, call);
        return os;
    }
    
    // Call recording
    std::vector<Call> calls;
    
    // Configurable return values
    uint64_t recordStartReturnValue = 1;
    uint64_t saveChatMessageReturnValue = 1;
    uint64_t createTerminalSessionReturnValue = 1;
    bool wasLastRunCrashedReturnValue = false;
    bool isActiveTerminalSessionReturnValue = true;
    std::vector<ChatMessageRecord> getChatHistoryReturnValue;
    std::vector<TerminalSession> getActiveTerminalSessionsReturnValue;
    std::vector<LLMProvider> getAllLLMProvidersReturnValue;
    std::optional<LLMProvider> getLLMProviderReturnValue;
    
    // Server run tracking
    uint64_t recordStart() override {
        this->calls.push_back(RecordStartCall{});
        return this->recordStartReturnValue;
    }
    
    void recordStop(uint64_t runId) override {
        this->calls.push_back(RecordStopCall{runId});
    }
    
    std::optional<ServerRun> getLastRun() override {
        return std::nullopt;
    }
    
    std::optional<ServerStop> getStopForRun(uint64_t /*runId*/) override {
        return std::nullopt;
    }
    
    bool wasLastRunCrashed() override {
        return this->wasLastRunCrashedReturnValue;
    }
    
    // Terminal session management
    uint64_t createTerminalSession(uint64_t serverRunId) override {
        this->calls.push_back(CreateTerminalSessionCall{serverRunId});
        return this->createTerminalSessionReturnValue;
    }
    
    void markTerminalSessionAsDeleted(uint64_t /*sessionId*/) override {}
    
    bool isActiveTerminalSession(uint64_t sessionId) override {
        this->calls.push_back(IsActiveTerminalSessionCall{sessionId});
        return this->isActiveTerminalSessionReturnValue;
    }
    
    std::optional<TerminalSession> getTerminalSession(uint64_t /*sessionId*/) override {
        return std::nullopt;
    }
    
    std::vector<TerminalSession> getActiveTerminalSessions() override {
        return this->getActiveTerminalSessionsReturnValue;
    }
    
    // LLM Provider management
    uint64_t addLLMProvider(const std::string& /*name*/, const std::string& /*type*/,
                            const std::string& /*url*/, const std::string& /*model*/,
                            const std::string& /*apiKey*/) override {
        return 1;
    }
    
    void updateLLMProvider(uint64_t /*id*/, const std::string& /*name*/,
                           const std::string& /*url*/, const std::string& /*model*/,
                           const std::string& /*apiKey*/) override {}
    
    void deleteLLMProvider(uint64_t /*id*/) override {}
    
    std::optional<LLMProvider> getLLMProvider(uint64_t /*id*/) override {
        return this->getLLMProviderReturnValue;
    }
    
    std::vector<LLMProvider> getAllLLMProviders() override {
        return this->getAllLLMProvidersReturnValue;
    }
    
    // Chat history management
    uint64_t saveChatMessage(uint64_t sessionId, const std::string& role, const std::string& content) override {
        this->calls.push_back(SaveChatMessageCall{sessionId, role, content});
        return this->saveChatMessageReturnValue++;
    }
    
    std::vector<ChatMessageRecord> getChatHistory(uint64_t sessionId) override {
        this->calls.push_back(GetChatHistoryCall{sessionId});
        return this->getChatHistoryReturnValue;
    }
    
    void clearChatHistory(uint64_t sessionId) override {
        this->calls.push_back(ClearChatHistoryCall{sessionId});
    }
};
