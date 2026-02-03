#include "MainWindow.h"
#include "ClientCoreWrapper.h"
#include "WelcomeView.h"
#include "ConnectingView.h"
#include "TerminalView.h"
#include <termihui/protocol/json_serialization.h>
#include <fmt/core.h>

using json = nlohmann::json;

MainWindow::MainWindow(ClientCoreWrapper& clientCore)
    : clientCore(clientCore)
    , currentState(WelcomeState{}) {

    this->setupUI();
    this->setupCallbacks();
    this->startPolling();
    std::visit([this](const auto& s) { this->updateUIForState(s); }, this->currentState);
}

MainWindow::~MainWindow() {
    this->stopPolling();
}

void MainWindow::setupUI() {
    this->set_title("TermiHUI");
    this->set_default_size(800, 600);

    // Create views
    this->welcomeView = std::make_unique<WelcomeView>();
    this->connectingView = std::make_unique<ConnectingView>();
    this->terminalView = std::make_unique<TerminalView>(this->clientCore);

    // Setup stack
    this->stack.set_transition_type(Gtk::StackTransitionType::CROSSFADE);
    this->stack.set_transition_duration(150);

    this->stack.add(*this->welcomeView, "welcome");
    this->stack.add(*this->connectingView, "connecting");
    this->stack.add(*this->terminalView, "terminal");

    this->set_child(this->stack);
}

void MainWindow::setupCallbacks() {
    this->welcomeView->setConnectCallback([this](const std::string& address) {
        this->onConnectRequested(address);
    });

    this->connectingView->setCancelCallback([this]() {
        this->onCancelRequested();
    });
}

void MainWindow::startPolling() {
    this->pollConnection = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &MainWindow::onPollTimeout),
        16  // ~60fps
    );
    fmt::print("[MainWindow] Polling started\n");
}

void MainWindow::stopPolling() {
    this->pollConnection.disconnect();
    fmt::print("[MainWindow] Polling stopped\n");
}

bool MainWindow::onPollTimeout() {
    this->clientCore.update();

    std::string event;
    while (!(event = this->clientCore.pollEvent()).empty()) {
        this->handleEvent(event);
    }

    return true;
}

void MainWindow::handleEvent(const std::string& event) {
    fmt::print("[MainWindow] Event: {}\n", event);

    try {
        auto j = json::parse(event);
        std::string type = j.at("type").get<std::string>();

        if (type == "connectionStateChanged") {
            std::string state = j.at("state").get<std::string>();
            this->handleConnectionStateChanged(state, j);

        } else if (type == "serverMessage") {
            this->handleServerMessage(j.at("data"));

        } else if (type == "error") {
            std::string message = j.value("message", "Unknown error");
            fmt::print(stderr, "[MainWindow] Error: {}\n", message);
            this->setState(ErrorState{message});
        }
    } catch (const json::exception& e) {
        fmt::print(stderr, "[MainWindow] JSON parse error: {}\n", e.what());
    }
}

void MainWindow::handleConnectionStateChanged(const std::string& state, const json& j) {
    fmt::print("[MainWindow] Connection state: {}\n", state);

    if (state == "connecting") {
        this->setState(ConnectingState{j.at("address").get<std::string>()});

    } else if (state == "connected") {
        this->setState(ConnectedState{j.at("address").get<std::string>()});

    } else if (state == "disconnected") {
        this->setState(WelcomeState{});

    } else {
        fmt::print(stderr, "[MainWindow] Unknown connection state: '{}'\n", state);
    }
}

void MainWindow::handleServerMessage(const json& j) {
    try {
        std::string_view type = j.at("type").get<std::string_view>();

        if (type == ConnectedMessage::type) {
            auto connectedMessage = j.get<ConnectedMessage>();
            this->terminalView->handleConnected(
                connectedMessage.serverVersion,
                connectedMessage.home.value_or("")
            );

        } else if (type == SessionsListMessage::type) {
            auto sessionsListMessage = j.get<SessionsListMessage>();
            uint64_t activeId = j.value("active_session_id", 0UL);
            this->terminalView->handleSessionsList(sessionsListMessage.sessions, activeId);

        } else if (type == HistoryMessage::type) {
            auto historyMessage = j.get<HistoryMessage>();
            this->terminalView->handleHistory(historyMessage.commands);

        } else if (type == OutputMessage::type) {
            auto outputMessage = j.get<OutputMessage>();
            this->terminalView->handleOutput(outputMessage.segments);

        } else if (type == CommandStartMessage::type) {
            auto commandStartMessage = j.get<CommandStartMessage>();
            this->terminalView->handleCommandStart(commandStartMessage.cwd.value_or(""));

        } else if (type == CommandEndMessage::type) {
            auto commandEndMessage = j.get<CommandEndMessage>();
            this->terminalView->handleCommandEnd(
                commandEndMessage.exitCode,
                commandEndMessage.cwd.value_or("")
            );

        } else if (type == CwdUpdateMessage::type) {
            auto cwdUpdateMessage = j.get<CwdUpdateMessage>();
            this->terminalView->handleCwdUpdate(cwdUpdateMessage.cwd);

        } else {
            fmt::print("[MainWindow] Unhandled server message type: {}\n", type);
        }

    } catch (const json::exception& e) {
        fmt::print(stderr, "[MainWindow] Server message parse error: {}\n", e.what());
    }
}

void MainWindow::setState(AppState newState) {
    this->currentState = std::move(newState);
    fmt::print("[MainWindow] State: {}\n", appStateName(this->currentState));
    std::visit([this](const auto& s) { this->updateUIForState(s); }, this->currentState);
}

// Overloaded UI update methods

void MainWindow::updateUIForState(const WelcomeState&) {
    this->set_title("TermiHUI");
    this->stack.set_visible_child("welcome");
    this->terminalView->clearState();
}

void MainWindow::updateUIForState(const ConnectingState& state) {
    this->set_title("Connecting...");
    this->connectingView->setServerAddress(state.serverAddress);
    this->stack.set_visible_child("connecting");
}

void MainWindow::updateUIForState(const ConnectedState& state) {
    this->set_title("TermiHUI - " + state.serverAddress);
    this->terminalView->setServerAddress(state.serverAddress);
    this->stack.set_visible_child("terminal");
}

void MainWindow::updateUIForState(const ErrorState& state) {
    fmt::print(stderr, "[MainWindow] Error: {}\n", state.message);
    this->set_title("TermiHUI");
    this->stack.set_visible_child("welcome");
    // TODO: Show error dialog
}

void MainWindow::onConnectRequested(const std::string& address) {
    fmt::print("[MainWindow] Connect requested: {}\n", address);
    std::string message = R"({"type":"connectButtonClicked","address":")" + address + R"("})";
    this->clientCore.sendMessage(message);
}

void MainWindow::onCancelRequested() {
    fmt::print("[MainWindow] Cancel requested\n");
    std::string message = R"({"type":"disconnectButtonClicked"})";
    this->clientCore.sendMessage(message);
}
