#pragma once

#include "TermihuiServerController.h"
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
    bool mockHandleExecuteMessage = true;
    bool mockHandleInputMessage = true;
    bool mockHandleCompletionMessage = true;
    bool mockHandleResizeMessage = true;
    
    void handleExecuteMessage(int clientId, uint64_t sessionId, const std::string& command) override {
        this->calls.push_back(ExecuteCall{.clientId = clientId, .sessionId = sessionId, .command = command});
        if (!this->mockHandleExecuteMessage) {
            this->TermihuiServerController::handleExecuteMessage(clientId, sessionId, command);
        }
    }
    
    void handleInputMessage(int clientId, uint64_t sessionId, const std::string& text) override {
        this->calls.push_back(InputCall{.clientId = clientId, .sessionId = sessionId, .text = text});
        if (!this->mockHandleInputMessage) {
            this->TermihuiServerController::handleInputMessage(clientId, sessionId, text);
        }
    }
    
    void handleCompletionMessage(int clientId, uint64_t sessionId, const std::string& text, int cursorPosition) override {
        this->calls.push_back(CompletionCall{.clientId = clientId, .sessionId = sessionId, .text = text, .cursorPosition = cursorPosition});
        if (!this->mockHandleCompletionMessage) {
            this->TermihuiServerController::handleCompletionMessage(clientId, sessionId, text, cursorPosition);
        }
    }
    
    void handleResizeMessage(int clientId, uint64_t sessionId, int cols, int rows) override {
        this->calls.push_back(ResizeCall{.clientId = clientId, .sessionId = sessionId, .cols = cols, .rows = rows});
        if (!this->mockHandleResizeMessage) {
            this->TermihuiServerController::handleResizeMessage(clientId, sessionId, cols, rows);
        }
    }
};
