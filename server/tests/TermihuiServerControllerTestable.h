#pragma once

#include "TermihuiServerController.h"
#include <variant>
#include <vector>
#include <string>

/**
 * Testable version of TermihuiServerController that records handler calls
 */
class TermihuiServerControllerTestable : public TermihuiServerController {
public:
    // Call record structures with default comparison (C++20)
    struct ExecuteCall {
        int clientId = 0;
        std::string command;
        auto operator<=>(const ExecuteCall&) const = default;
    };

    struct InputCall {
        int clientId = 0;
        std::string text;
        auto operator<=>(const InputCall&) const = default;
    };

    struct CompletionCall {
        int clientId = 0;
        std::string text;
        int cursorPosition = 0;
        auto operator<=>(const CompletionCall&) const = default;
    };

    struct ResizeCall {
        int clientId = 0;
        int cols = 0;
        int rows = 0;
        auto operator<=>(const ResizeCall&) const = default;
    };

    using Call = std::variant<ExecuteCall, InputCall, CompletionCall, ResizeCall>;

    TermihuiServerControllerTestable() : TermihuiServerController(0, "127.0.0.1") {}
    
    // Call recording
    std::vector<Call> calls;
    
    // Mock flags (true = mock, false = call real implementation)
    bool mockHandleExecuteMessage = true;
    bool mockHandleInputMessage = true;
    bool mockHandleCompletionMessage = true;
    bool mockHandleResizeMessage = true;
    
    void handleExecuteMessage(int clientId, const std::string& command) override {
        this->calls.push_back(ExecuteCall{clientId, command});
        if (!this->mockHandleExecuteMessage) {
            TermihuiServerController::handleExecuteMessage(clientId, command);
        }
    }
    
    void handleInputMessage(int clientId, const std::string& text) override {
        this->calls.push_back(InputCall{clientId, text});
        if (!this->mockHandleInputMessage) {
            TermihuiServerController::handleInputMessage(clientId, text);
        }
    }
    
    void handleCompletionMessage(int clientId, const std::string& text, int cursorPosition) override {
        this->calls.push_back(CompletionCall{clientId, text, cursorPosition});
        if (!this->mockHandleCompletionMessage) {
            TermihuiServerController::handleCompletionMessage(clientId, text, cursorPosition);
        }
    }
    
    void handleResizeMessage(int clientId, int cols, int rows) override {
        this->calls.push_back(ResizeCall{clientId, cols, rows});
        if (!this->mockHandleResizeMessage) {
            TermihuiServerController::handleResizeMessage(clientId, cols, rows);
        }
    }
};
