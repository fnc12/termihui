#pragma once

#include "termihui/client_core.h"
#include <variant>
#include <vector>
#include <string>

namespace termihui {

/**
 * Testable version of ClientCoreController that records handler calls
 */
class ClientCoreControllerTestable : public ClientCoreController {
public:
    // Call record structures with default comparison (C++20)
    struct ConnectButtonClickedCall {
        std::string address;
        auto operator<=>(const ConnectButtonClickedCall&) const = default;
    };

    struct RequestReconnectCall {
        std::string address;
        auto operator<=>(const RequestReconnectCall&) const = default;
    };

    struct DisconnectButtonClickedCall {
        auto operator<=>(const DisconnectButtonClickedCall&) const = default;
    };

    struct ExecuteCommandCall {
        std::string command;
        auto operator<=>(const ExecuteCommandCall&) const = default;
    };

    struct SendInputCall {
        std::string text;
        auto operator<=>(const SendInputCall&) const = default;
    };

    struct ResizeCall {
        int cols = 0;
        int rows = 0;
        auto operator<=>(const ResizeCall&) const = default;
    };

    struct RequestCompletionCall {
        std::string text;
        int cursorPosition = 0;
        auto operator<=>(const RequestCompletionCall&) const = default;
    };

    using Call = std::variant<
        ConnectButtonClickedCall,
        RequestReconnectCall,
        DisconnectButtonClickedCall,
        ExecuteCommandCall,
        SendInputCall,
        ResizeCall,
        RequestCompletionCall
    >;

    // Call recording
    std::vector<Call> calls;

    // Mock flags (true = mock, false = call real implementation)
    bool mockHandleConnectButtonClicked = true;
    bool mockHandleRequestReconnect = true;
    bool mockHandleDisconnectButtonClicked = true;
    bool mockHandleExecuteCommand = true;
    bool mockHandleSendInput = true;
    bool mockHandleResize = true;
    bool mockHandleRequestCompletion = true;

    void handleConnectButtonClicked(std::string_view address) override;
    void handleRequestReconnect(std::string_view address) override;
    void handleDisconnectButtonClicked() override;
    void handleExecuteCommand(std::string_view command) override;
    void handleSendInput(std::string_view text) override;
    void handleResize(int cols, int rows) override;
    void handleRequestCompletion(std::string_view text, int cursorPosition) override;
};

} // namespace termihui
