#pragma once

#include <string>

/// Simple C++ wrapper for TermiHUI client core (no active logic)
class ClientCoreWrapper {
public:
    ClientCoreWrapper();
    ~ClientCoreWrapper();

    // Disable copying
    ClientCoreWrapper(const ClientCoreWrapper&) = delete;
    ClientCoreWrapper& operator=(const ClientCoreWrapper&) = delete;

    /// Get client core version
    static std::string getVersion();

    /// Initialize client core
    bool initialize();

    /// Shutdown client core
    void shutdown();

    /// Check if initialized
    bool isInitialized() const;

    /// Send JSON message to client core
    std::string sendMessage(const std::string& message);

    /// Process WebSocket events (call regularly from main loop)
    void update();

    /// Poll single event from queue
    /// @return event JSON string or empty if no events
    std::string pollEvent();

    /// Get count of pending events
    int pendingEventsCount() const;
};
