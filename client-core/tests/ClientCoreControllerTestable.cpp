#include "ClientCoreControllerTestable.h"

namespace termihui {

void ClientCoreControllerTestable::handleConnectButtonClicked(std::string_view address) {
    this->calls.push_back(ConnectButtonClickedCall{std::string(address)});
    if (!this->mockHandleConnectButtonClicked) {
        this->ClientCoreController::handleConnectButtonClicked(address);
    }
}

void ClientCoreControllerTestable::handleRequestReconnect(std::string_view address) {
    this->calls.push_back(RequestReconnectCall{std::string(address)});
    if (!this->mockHandleRequestReconnect) {
        this->ClientCoreController::handleRequestReconnect(address);
    }
}

void ClientCoreControllerTestable::handleDisconnectButtonClicked() {
    this->calls.push_back(DisconnectButtonClickedCall{});
    if (!this->mockHandleDisconnectButtonClicked) {
        this->ClientCoreController::handleDisconnectButtonClicked();
    }
}

void ClientCoreControllerTestable::handleExecuteCommand(std::string_view command) {
    this->calls.push_back(ExecuteCommandCall{std::string(command)});
    if (!this->mockHandleExecuteCommand) {
        this->ClientCoreController::handleExecuteCommand(command);
    }
}

void ClientCoreControllerTestable::handleSendInput(std::string_view text) {
    this->calls.push_back(SendInputCall{std::string(text)});
    if (!this->mockHandleSendInput) {
        this->ClientCoreController::handleSendInput(text);
    }
}

void ClientCoreControllerTestable::handleResize(int cols, int rows) {
    this->calls.push_back(ResizeCall{cols, rows});
    if (!this->mockHandleResize) {
        this->ClientCoreController::handleResize(cols, rows);
    }
}

void ClientCoreControllerTestable::handleRequestCompletion(std::string_view text, int cursorPosition) {
    this->calls.push_back(RequestCompletionCall{std::string(text), cursorPosition});
    if (!this->mockHandleRequestCompletion) {
        this->ClientCoreController::handleRequestCompletion(text, cursorPosition);
    }
}

} // namespace termihui

