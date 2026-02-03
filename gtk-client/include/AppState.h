#pragma once

#include <string>
#include <variant>

/// Application states for navigation management (like macOS AppState)
struct WelcomeState {};

struct ConnectingState {
    std::string serverAddress;
};

struct ConnectedState {
    std::string serverAddress;
};

struct ErrorState {
    std::string message;
};

using AppState = std::variant<WelcomeState, ConnectingState, ConnectedState, ErrorState>;

/// Overloaded helper for state name (compile-time checked)
inline std::string appStateName(const WelcomeState&) { return "Welcome"; }
inline std::string appStateName(const ConnectingState&) { return "Connecting"; }
inline std::string appStateName(const ConnectedState&) { return "Connected"; }
inline std::string appStateName(const ErrorState&) { return "Error"; }

inline std::string appStateName(const AppState& state) {
    return std::visit([](const auto& s) { return appStateName(s); }, state);
}
