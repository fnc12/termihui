#pragma once

#include "termihui/client_core.h"
#include "termihui/websocket_client_controller.h"
#include <cstdint>
#include <variant>
#include <vector>
#include <string>

namespace termihui {

/**
 * Testable version of ClientCoreController that records handler calls
 */
class ClientCoreControllerTestable : public ClientCoreController {
public:
    // Inherit constructors
    using ClientCoreController::ClientCoreController;
    
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

    struct CreateSessionCall {
        auto operator<=>(const CreateSessionCall&) const = default;
    };

    struct CloseSessionCall {
        uint64_t sessionId = 0;
        auto operator<=>(const CloseSessionCall&) const = default;
    };

    struct SwitchSessionCall {
        uint64_t sessionId = 0;
        auto operator<=>(const SwitchSessionCall&) const = default;
    };

    struct ListSessionsCall {
        auto operator<=>(const ListSessionsCall&) const = default;
    };

    // WebSocket event call records
    struct OpenEventCall {
        auto operator<=>(const OpenEventCall&) const = default;
    };

    struct MessageEventCall {
        std::string message;
        auto operator<=>(const MessageEventCall&) const = default;
    };

    struct CloseEventCall {
        auto operator<=>(const CloseEventCall&) const = default;
    };

    struct ErrorEventCall {
        std::string error;
        auto operator<=>(const ErrorEventCall&) const = default;
    };

    using Call = std::variant<
        ConnectButtonClickedCall,
        RequestReconnectCall,
        DisconnectButtonClickedCall,
        ExecuteCommandCall,
        SendInputCall,
        ResizeCall,
        RequestCompletionCall,
        CreateSessionCall,
        CloseSessionCall,
        SwitchSessionCall,
        ListSessionsCall,
        OpenEventCall,
        MessageEventCall,
        CloseEventCall,
        ErrorEventCall
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
    bool mockHandleCreateSession = true;
    bool mockHandleCloseSession = true;
    bool mockHandleSwitchSession = true;
    bool mockHandleListSessions = true;
    bool mockHandleWebSocketEvent = true;

    std::string handleConnectButtonClicked(std::string_view address) override;
    std::string handleRequestReconnect(std::string_view address) override;
    std::string handleDisconnectButtonClicked() override;
    std::string handleExecuteCommand(std::string_view command) override;
    std::string handleSendInput(std::string_view text) override;
    std::string handleResize(int cols, int rows) override;
    std::string handleRequestCompletion(std::string_view text, int cursorPosition) override;
    std::string handleCreateSession() override;
    std::string handleCloseSession(uint64_t sessionId) override;
    std::string handleSwitchSession(uint64_t sessionId) override;
    std::string handleListSessions() override;
    
    // WebSocket event handlers
    void handleWebSocketEvent(const WebSocketClientController::OpenEvent& openEvent) override;
    void handleWebSocketEvent(const WebSocketClientController::MessageEvent& messageEvent) override;
    void handleWebSocketEvent(const WebSocketClientController::CloseEvent& closeEvent) override;
    void handleWebSocketEvent(const WebSocketClientController::ErrorEvent& errorEvent) override;
};

} // namespace termihui
