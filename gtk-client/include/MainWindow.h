#pragma once

#include <gtkmm.h>
#include <memory>
#include <hv/json.hpp>
#include "AppState.h"

class ClientCoreWrapper;
class WelcomeView;
class ConnectingView;
class TerminalView;

/// Main window - manages app state and switches views (like RootViewController)
class MainWindow : public Gtk::ApplicationWindow {
public:
    explicit MainWindow(ClientCoreWrapper& clientCore);
    ~MainWindow() override;

private:
    void setupUI();
    void setupCallbacks();
    void startPolling();
    void stopPolling();

    bool onPollTimeout();
    void handleEvent(const std::string& event);
    void handleConnectionStateChanged(const std::string& state, const nlohmann::json& j);
    void handleServerMessage(const nlohmann::json& j);

    void setState(AppState newState);

    // Overloaded UI update methods (compile-time checked)
    void updateUIForState(const WelcomeState& state);
    void updateUIForState(const ConnectingState& state);
    void updateUIForState(const ConnectedState& state);
    void updateUIForState(const ErrorState& state);

    // Callbacks from child views
    void onConnectRequested(const std::string& address);
    void onCancelRequested();

    ClientCoreWrapper& clientCore;
    sigc::connection pollConnection;
    AppState currentState;

    // UI
    Gtk::Stack stack;
    std::unique_ptr<WelcomeView> welcomeView;
    std::unique_ptr<ConnectingView> connectingView;
    std::unique_ptr<TerminalView> terminalView;
};
