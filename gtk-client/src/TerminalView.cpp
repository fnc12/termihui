#include "TerminalView.h"
#include "ClientCoreWrapper.h"
#include "CommandBlockWidget.h"
#include "PangoUtils.h"
#include <hv/json.hpp>
#include <fmt/core.h>
#include <gdk/gdkkeysyms.h>

using json = nlohmann::json;

TerminalView::TerminalView(ClientCoreWrapper& clientCore)
    : clientCore(clientCore) {

    this->set_orientation(Gtk::Orientation::VERTICAL);
    this->setupUI();
    this->setupInputHandlers();
}

void TerminalView::setupUI() {
    // Header bar
    this->headerBox.set_orientation(Gtk::Orientation::HORIZONTAL);
    this->headerBox.set_spacing(10);
    this->headerBox.set_margin(10);
    this->headerBox.add_css_class("header-box");

    this->sessionLabel.set_text("Session");
    this->sessionLabel.set_hexpand(true);
    this->sessionLabel.set_halign(Gtk::Align::START);
    this->sessionLabel.add_css_class("session-label");

    this->disconnectButton.set_label("Disconnect");
    this->disconnectButton.signal_clicked().connect(
        sigc::mem_fun(*this, &TerminalView::onDisconnectClicked)
    );

    this->headerBox.append(this->sessionLabel);
    this->headerBox.append(this->disconnectButton);

    // Scrolled window with command blocks
    this->scrolledWindow.set_vexpand(true);
    this->scrolledWindow.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);

    this->blocksContainer.set_orientation(Gtk::Orientation::VERTICAL);
    this->blocksContainer.set_spacing(8);
    this->blocksContainer.add_css_class("terminal-blocks");

    this->scrolledWindow.set_child(this->blocksContainer);

    // Input container - vertical layout like macOS
    this->inputBox.set_orientation(Gtk::Orientation::VERTICAL);
    this->inputBox.set_spacing(4);
    this->inputBox.set_margin(10);
    this->inputBox.add_css_class("input-box");

    // CWD label on top
    this->cwdLabel.set_text("~");
    this->cwdLabel.add_css_class("cwd-label");
    this->cwdLabel.set_halign(Gtk::Align::START);
    this->cwdLabel.set_ellipsize(Pango::EllipsizeMode::START);

    // Entry row: entry + send button
    this->entryRow.set_orientation(Gtk::Orientation::HORIZONTAL);
    this->entryRow.set_spacing(8);

    this->commandEntry.set_hexpand(true);
    this->commandEntry.set_placeholder_text("Enter command...");

    this->sendButton.set_label("⬆");
    this->sendButton.add_css_class("send-button");
    this->sendButton.set_size_request(36, 36);
    this->sendButton.set_valign(Gtk::Align::CENTER);
    this->sendButton.set_vexpand(false);
    this->sendButton.signal_clicked().connect(
        sigc::mem_fun(*this, &TerminalView::onSendClicked)
    );

    this->entryRow.append(this->commandEntry);
    this->entryRow.append(this->sendButton);

    this->inputBox.append(this->cwdLabel);
    this->inputBox.append(this->entryRow);

    // Assemble
    this->append(this->headerBox);
    this->append(this->scrolledWindow);
    this->append(this->inputBox);
}

void TerminalView::setupInputHandlers() {
    auto keyController = Gtk::EventControllerKey::create();
    keyController->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) -> bool {
            if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
                this->onSendClicked();
                return true;
            }
            return false;
        },
        false
    );
    this->commandEntry.add_controller(keyController);
}

void TerminalView::setServerAddress(std::string address) {
    this->serverAddress = std::move(address);
}

void TerminalView::setDisconnectCallback(DisconnectCallback callback) {
    this->disconnectCallback = std::move(callback);
}

void TerminalView::onSendClicked() {
    std::string command = this->commandEntry.get_text();
    if (command.empty()) {
        return;
    }

    this->commandEntry.set_text("");
    fmt::print("[TerminalView] Sending command: {}\n", command);

    // Client-core internal protocol: executeCommand with just command field
    json message = {
        {"type", "executeCommand"},
        {"command", command}
    };
    this->clientCore.sendMessage(message.dump());
}

void TerminalView::onDisconnectClicked() {
    fmt::print("[TerminalView] Disconnect clicked\n");
    std::string message = R"({"type":"disconnectButtonClicked"})";
    this->clientCore.sendMessage(message);

    if (this->disconnectCallback) {
        this->disconnectCallback();
    }
}

void TerminalView::scrollToBottom() {
    // Scroll to bottom after adding content
    auto adj = this->scrolledWindow.get_vadjustment();
    adj->set_value(adj->get_upper() - adj->get_page_size());
}

void TerminalView::updateCwdDisplay(const std::string& cwd) {
    this->currentCwd = cwd;
    std::string displayCwd = cwd;
    if (!this->serverHome.empty() && cwd.find(this->serverHome) == 0) {
        displayCwd = "~" + cwd.substr(this->serverHome.length());
    }
    this->cwdLabel.set_text(displayCwd);
}

void TerminalView::clearState() {
    // Clear command blocks
    while (auto child = this->blocksContainer.get_first_child()) {
        this->blocksContainer.remove(*child);
    }
    this->commandBlocks.clear();
    this->currentBlockIndex = std::nullopt;

    this->currentCwd.clear();
    this->serverHome.clear();
    this->activeSessionId = 0;
    this->cwdLabel.set_text("~");
    this->sessionLabel.set_text("Session");
}

// Server message handlers

void TerminalView::handleConnected(const std::string& serverVersion, const std::string& home) {
    fmt::print("[TerminalView] Connected: version={}, home={}\n", serverVersion, home);
    this->serverHome = home;
}

void TerminalView::handleSessionsList(const std::vector<SessionInfo>& sessions, uint64_t activeSessionId) {
    fmt::print("[TerminalView] Sessions: {} sessions, active={}\n", sessions.size(), activeSessionId);
    this->activeSessionId = activeSessionId;
    this->sessionLabel.set_text(fmt::format("Session #{}", activeSessionId));
}

void TerminalView::handleHistory(const std::vector<CommandRecord>& commands) {
    fmt::print("[TerminalView] Loading history: {} commands\n", commands.size());

    // Clear existing blocks
    while (auto child = this->blocksContainer.get_first_child()) {
        this->blocksContainer.remove(*child);
    }
    this->commandBlocks.clear();
    this->currentBlockIndex = std::nullopt;

    // Load history
    for (const auto& record : commands) {
        this->addCommandBlock(
            record.id,
            record.command,
            record.segments,
            record.isFinished,
            record.exitCode,
            record.cwdStart
        );

        // Track unfinished command
        if (!record.isFinished) {
            this->currentBlockIndex = this->commandBlocks.size() - 1;
        }

        // Update CWD from last command
        if (!record.cwdEnd.empty()) {
            this->updateCwdDisplay(record.cwdEnd);
        } else if (!record.cwdStart.empty()) {
            this->updateCwdDisplay(record.cwdStart);
        }
    }

    // Scroll to bottom after loading
    Glib::signal_idle().connect_once([this]() {
        this->scrollToBottom();
    });
}

void TerminalView::handleOutput(const std::vector<StyledSegment>& segments) {
    if (segments.empty()) {
        return;
    }

    if (this->currentBlockIndex.has_value()) {
        this->appendToCurrentBlock(segments);
    } else {
        // Output without command - create standalone block
        this->addCommandBlock(std::nullopt, "", segments, false, std::nullopt, "");
        this->currentBlockIndex = this->commandBlocks.size() - 1;
    }

    Glib::signal_idle().connect_once([this]() {
        this->scrollToBottom();
    });
}

void TerminalView::handleCommandStart(const std::string& cwd) {
    fmt::print("[TerminalView] Command start, cwd={}\n", cwd);

    // Create new block for the command
    this->addCommandBlock(std::nullopt, "", {}, false, std::nullopt, cwd);
    this->currentBlockIndex = this->commandBlocks.size() - 1;

    if (!cwd.empty()) {
        this->updateCwdDisplay(cwd);
    }
}

void TerminalView::handleCommandEnd(int exitCode, const std::string& cwd) {
    fmt::print("[TerminalView] Command end, exitCode={}, cwd={}\n", exitCode, cwd);
    this->finishCurrentBlock(exitCode, cwd);

    if (!cwd.empty()) {
        this->updateCwdDisplay(cwd);
    }
}

void TerminalView::handleCwdUpdate(const std::string& cwd) {
    this->updateCwdDisplay(cwd);
}

// Command block management

void TerminalView::addCommandBlock(
    std::optional<uint64_t> commandId,
    const std::string& command,
    const std::vector<StyledSegment>& segments,
    bool isFinished,
    std::optional<int> exitCode,
    const std::string& cwdStart
) {
    // Add to model
    CommandBlock block;
    block.commandId = commandId;
    block.command = command;
    block.outputSegments = segments;
    block.isFinished = isFinished;
    block.exitCode = exitCode;
    block.cwdStart = cwdStart;
    this->commandBlocks.push_back(block);

    // Create widget
    auto widget = Gtk::make_managed<CommandBlockWidget>();
    widget->configure(commandId, command, segments, isFinished, exitCode, cwdStart, this->serverHome);
    this->blocksContainer.append(*widget);
}

void TerminalView::appendToCurrentBlock(const std::vector<StyledSegment>& segments) {
    if (!this->currentBlockIndex.has_value()) {
        return;
    }

    size_t idx = *this->currentBlockIndex;
    if (idx >= this->commandBlocks.size()) {
        return;
    }

    // Update model
    auto& block = this->commandBlocks[idx];
    block.outputSegments.insert(block.outputSegments.end(), segments.begin(), segments.end());

    // Update widget - find by index
    size_t widgetIdx = 0;
    for (auto child = this->blocksContainer.get_first_child(); child; child = child->get_next_sibling()) {
        if (widgetIdx == idx) {
            if (auto* blockWidget = dynamic_cast<CommandBlockWidget*>(child)) {
                blockWidget->configure(
                    block.commandId, block.command, block.outputSegments,
                    block.isFinished, block.exitCode, block.cwdStart, this->serverHome
                );
            }
            break;
        }
        widgetIdx++;
    }
}

void TerminalView::finishCurrentBlock(int exitCode, const std::string& cwd) {
    if (!this->currentBlockIndex.has_value()) {
        return;
    }

    size_t idx = *this->currentBlockIndex;
    if (idx >= this->commandBlocks.size()) {
        return;
    }

    // Update model
    auto& block = this->commandBlocks[idx];
    block.isFinished = true;
    block.exitCode = exitCode;
    block.cwdEnd = cwd;

    // Update widget
    size_t widgetIdx = 0;
    for (auto child = this->blocksContainer.get_first_child(); child; child = child->get_next_sibling()) {
        if (widgetIdx == idx) {
            if (auto* blockWidget = dynamic_cast<CommandBlockWidget*>(child)) {
                blockWidget->configure(
                    block.commandId, block.command, block.outputSegments,
                    block.isFinished, block.exitCode, block.cwdStart, this->serverHome
                );
            }
            break;
        }
        widgetIdx++;
    }

    this->currentBlockIndex = std::nullopt;
}
