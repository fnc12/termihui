#pragma once

#include "TermihuiServerController.h"
#include <termihui/protocol/protocol.h>
#include <variant>
#include <vector>
#include <string>
#include <memory>


/**
 * Testable version of TermihuiServerController that records handler calls
 */
class TermihuiServerControllerTestable : public TermihuiServerController {
public:
    // Call record structures with default comparison (C++20)
    struct ExecuteCall {
        int clientId = 0;
        uint64_t sessionId = 0;
        std::string command;
        auto operator<=>(const ExecuteCall&) const = default;
    };

    struct InputCall {
        int clientId = 0;
        uint64_t sessionId = 0;
        std::string text;
        auto operator<=>(const InputCall&) const = default;
    };

    struct CompletionCall {
        int clientId = 0;
        uint64_t sessionId = 0;
        std::string text;
        int cursorPosition = 0;
        auto operator<=>(const CompletionCall&) const = default;
    };

    struct ResizeCall {
        int clientId = 0;
        uint64_t sessionId = 0;
        int cols = 0;
        int rows = 0;
        auto operator<=>(const ResizeCall&) const = default;
    };

    using Call = std::variant<ExecuteCall, InputCall, CompletionCall, ResizeCall>;

    explicit TermihuiServerControllerTestable(std::unique_ptr<WebSocketServer> webSocketServer)
        : TermihuiServerController(std::move(webSocketServer))
    {
    }
    
    // Call recording
    std::vector<Call> calls;
    
    // Mock flags (true = mock, false = call real implementation)
    bool mockExecute = true;
    bool mockInput = true;
    bool mockCompletion = true;
    bool mockResize = true;
    
    void handleMessageFromClient(int clientId, const ExecuteMessage& message) override {
        this->calls.push_back(ExecuteCall{.clientId = clientId, .sessionId = message.sessionId, .command = message.command});
        if (!this->mockExecute) {
            this->TermihuiServerController::handleMessageFromClient(clientId, message);
        }
    }
    
    void handleMessageFromClient(int clientId, const InputMessage& message) override {
        this->calls.push_back(InputCall{.clientId = clientId, .sessionId = message.sessionId, .text = message.text});
        if (!this->mockInput) {
            this->TermihuiServerController::handleMessageFromClient(clientId, message);
        }
    }
    
    void handleMessageFromClient(int clientId, const CompletionMessage& message) override {
        this->calls.push_back(CompletionCall{.clientId = clientId, .sessionId = message.sessionId, .text = message.text, .cursorPosition = message.cursorPosition});
        if (!this->mockCompletion) {
            this->TermihuiServerController::handleMessageFromClient(clientId, message);
        }
    }
    
    void handleMessageFromClient(int clientId, const ResizeMessage& message) override {
        this->calls.push_back(ResizeCall{.clientId = clientId, .sessionId = message.sessionId, .cols = message.cols, .rows = message.rows});
        if (!this->mockResize) {
            this->TermihuiServerController::handleMessageFromClient(clientId, message);
        }
    }
};
