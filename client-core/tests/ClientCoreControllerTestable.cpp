#include "ClientCoreControllerTestable.h"

namespace termihui {

std::string ClientCoreControllerTestable::handleConnectButtonClicked(std::string_view address) {
    this->calls.push_back(ConnectButtonClickedCall{std::string(address)});
    if (!this->mockHandleConnectButtonClicked) {
        return this->ClientCoreController::handleConnectButtonClicked(address);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleRequestReconnect(std::string_view address) {
    this->calls.push_back(RequestReconnectCall{std::string(address)});
    if (!this->mockHandleRequestReconnect) {
        return this->ClientCoreController::handleRequestReconnect(address);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleDisconnectButtonClicked() {
    this->calls.push_back(DisconnectButtonClickedCall{});
    if (!this->mockHandleDisconnectButtonClicked) {
        return this->ClientCoreController::handleDisconnectButtonClicked();
    }
    return "";
}

std::string ClientCoreControllerTestable::handleExecuteCommand(std::string_view command) {
    this->calls.push_back(ExecuteCommandCall{std::string(command)});
    if (!this->mockHandleExecuteCommand) {
        return this->ClientCoreController::handleExecuteCommand(command);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleSendInput(std::string_view text) {
    this->calls.push_back(SendInputCall{std::string(text)});
    if (!this->mockHandleSendInput) {
        return this->ClientCoreController::handleSendInput(text);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleResize(int cols, int rows) {
    this->calls.push_back(ResizeCall{cols, rows});
    if (!this->mockHandleResize) {
        return this->ClientCoreController::handleResize(cols, rows);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleRequestCompletion(std::string_view text, int cursorPosition) {
    this->calls.push_back(RequestCompletionCall{std::string(text), cursorPosition});
    if (!this->mockHandleRequestCompletion) {
        return this->ClientCoreController::handleRequestCompletion(text, cursorPosition);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleCreateSession() {
    this->calls.push_back(CreateSessionCall{});
    if (!this->mockHandleCreateSession) {
        return this->ClientCoreController::handleCreateSession();
    }
    return "";
}

std::string ClientCoreControllerTestable::handleCloseSession(uint64_t sessionId) {
    this->calls.push_back(CloseSessionCall{sessionId});
    if (!this->mockHandleCloseSession) {
        return this->ClientCoreController::handleCloseSession(sessionId);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleSwitchSession(uint64_t sessionId) {
    this->calls.push_back(SwitchSessionCall{sessionId});
    if (!this->mockHandleSwitchSession) {
        return this->ClientCoreController::handleSwitchSession(sessionId);
    }
    return "";
}

std::string ClientCoreControllerTestable::handleListSessions() {
    this->calls.push_back(ListSessionsCall{});
    if (!this->mockHandleListSessions) {
        return this->ClientCoreController::handleListSessions();
    }
    return "";
}

} // namespace termihui
