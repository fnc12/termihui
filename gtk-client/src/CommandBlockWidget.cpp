#include "CommandBlockWidget.h"
#include "PangoUtils.h"

CommandBlockWidget::CommandBlockWidget() {
    this->set_orientation(Gtk::Orientation::VERTICAL);
    this->set_spacing(4);
    this->add_css_class("command-block");

    // CWD label - purple like macOS
    this->cwdLabel.set_halign(Gtk::Align::START);
    this->cwdLabel.set_margin_start(12);
    this->cwdLabel.set_margin_end(12);
    this->cwdLabel.set_margin_top(8);
    this->cwdLabel.add_css_class("cwd-label");
    this->cwdLabel.set_ellipsize(Pango::EllipsizeMode::START);

    // Header label - command with "$ " prefix
    this->headerLabel.set_halign(Gtk::Align::START);
    this->headerLabel.set_margin_start(12);
    this->headerLabel.set_margin_end(12);
    this->headerLabel.set_margin_top(4);
    this->headerLabel.set_wrap(true);
    this->headerLabel.set_selectable(true);
    this->headerLabel.add_css_class("command-header");

    // Output label - styled terminal output
    this->outputLabel.set_halign(Gtk::Align::START);
    this->outputLabel.set_valign(Gtk::Align::START);
    this->outputLabel.set_margin_start(12);
    this->outputLabel.set_margin_end(12);
    this->outputLabel.set_margin_top(4);
    this->outputLabel.set_margin_bottom(12);
    this->outputLabel.set_wrap(true);
    this->outputLabel.set_wrap_mode(Pango::WrapMode::CHAR);
    this->outputLabel.set_selectable(true);
    this->outputLabel.set_use_markup(true);
    this->outputLabel.add_css_class("output-text");

    this->append(this->cwdLabel);
    this->append(this->headerLabel);
    this->append(this->outputLabel);
}

void CommandBlockWidget::configure(
    std::optional<uint64_t> commandId,
    const std::string& command,
    const std::vector<StyledSegment>& outputSegments,
    bool isFinished,
    std::optional<int> exitCode,
    const std::string& cwdStart,
    const std::string& serverHome
) {
    this->commandId = commandId;

    // CWD label
    if (!cwdStart.empty()) {
        std::string displayCwd = cwdStart;
        // Shorten home path to ~
        if (!serverHome.empty() && cwdStart.find(serverHome) == 0) {
            displayCwd = "~" + cwdStart.substr(serverHome.length());
        }
        this->cwdLabel.set_text(displayCwd);
        this->cwdLabel.set_visible(true);
    } else {
        this->cwdLabel.set_visible(false);
    }

    // Header (command)
    if (!command.empty()) {
        this->headerLabel.set_markup("<b>$ " + Glib::Markup::escape_text(command) + "</b>");
        this->headerLabel.set_visible(true);
    } else {
        this->headerLabel.set_visible(false);
    }

    // Output
    if (!outputSegments.empty()) {
        std::string markup = segmentsToPangoMarkup(outputSegments);
        this->outputLabel.set_markup(markup);
        this->outputLabel.set_visible(true);
    } else {
        this->outputLabel.set_visible(false);
    }
}
