#pragma once

#include <gtkmm.h>
#include <termihui/text_style.h>
#include <optional>
#include <vector>
#include <cstdint>

/// Widget displaying a single command block (like CommandBlockItem in macOS)
class CommandBlockWidget : public Gtk::Box {
public:
    CommandBlockWidget();

    /// Configure the block with data
    void configure(
        std::optional<uint64_t> commandId,
        const std::string& command,
        const std::vector<StyledSegment>& outputSegments,
        bool isFinished,
        std::optional<int> exitCode,
        const std::string& cwdStart,
        const std::string& serverHome
    );

    /// Get command ID (for context menu actions)
    std::optional<uint64_t> getCommandId() const { return this->commandId; }

private:
    std::optional<uint64_t> commandId;

    Gtk::Label cwdLabel;
    Gtk::Label headerLabel;
    Gtk::Label outputLabel;
};
