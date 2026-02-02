#pragma once

#include <gtkmm.h>
#include <termihui/text_style.h>
#include <termihui/protocol/server_messages.h>
#include <functional>
#include <vector>
#include <optional>
#include <cstdint>

class ClientCoreWrapper;
class CommandBlockWidget;

/// Terminal screen (like TerminalViewController)
class TerminalView : public Gtk::Box {
public:
    using DisconnectCallback = std::function<void()>;

    explicit TerminalView(ClientCoreWrapper& clientCore);

    void setServerAddress(std::string address);
    void setDisconnectCallback(DisconnectCallback callback);

    // Server message handlers
    void handleConnected(const std::string& serverVersion, const std::string& home);
    void handleSessionsList(const std::vector<SessionInfo>& sessions, uint64_t activeSessionId);
    void handleHistory(const std::vector<CommandRecord>& commands);
    void handleOutput(const std::vector<StyledSegment>& segments);
    void handleCommandStart(const std::string& cwd);
    void handleCommandEnd(int exitCode, const std::string& cwd);
    void handleCwdUpdate(const std::string& cwd);

    /// Clear all state (on disconnect)
    void clearState();

private:
    void setupUI();
    void setupInputHandlers();

    void onSendClicked();
    void onDisconnectClicked();

    void scrollToBottom();
    void updateCwdDisplay(const std::string& cwd);

    // Command block management
    void addCommandBlock(
        std::optional<uint64_t> commandId,
        const std::string& command,
        const std::vector<StyledSegment>& segments,
        bool isFinished,
        std::optional<int> exitCode,
        const std::string& cwdStart
    );
    void appendToCurrentBlock(const std::vector<StyledSegment>& segments);
    void finishCurrentBlock(int exitCode, const std::string& cwd);

    ClientCoreWrapper& clientCore;
    DisconnectCallback disconnectCallback;

    std::string serverAddress;
    std::string serverHome;
    std::string currentCwd;
    uint64_t activeSessionId = 0;

    // Command blocks model
    struct CommandBlock {
        std::optional<uint64_t> commandId;
        std::string command;
        std::vector<StyledSegment> outputSegments;
        bool isFinished = false;
        std::optional<int> exitCode;
        std::string cwdStart;
        std::string cwdEnd;
    };
    std::vector<CommandBlock> commandBlocks;
    std::optional<size_t> currentBlockIndex;

    // UI components
    Gtk::Box headerBox;
    Gtk::Label sessionLabel;
    Gtk::Button disconnectButton;

    Gtk::ScrolledWindow scrolledWindow;
    Gtk::Box blocksContainer;

    Gtk::Box inputBox;
    Gtk::Label cwdLabel;
    Gtk::Box entryRow;
    Gtk::Entry commandEntry;
    Gtk::Button sendButton;
};
