#include "ConnectingView.h"

ConnectingView::ConnectingView() {
    this->set_orientation(Gtk::Orientation::VERTICAL);
    this->set_valign(Gtk::Align::CENTER);
    this->set_halign(Gtk::Align::CENTER);

    // Content box
    this->contentBox.set_orientation(Gtk::Orientation::VERTICAL);
    this->contentBox.set_spacing(16);
    this->contentBox.set_margin(40);

    // Spinner
    this->spinner.set_size_request(48, 48);
    this->spinner.set_halign(Gtk::Align::CENTER);
    this->spinner.start();

    // Status label
    this->statusLabel.set_text("Connecting...");
    this->statusLabel.set_halign(Gtk::Align::CENTER);

    // Cancel button
    this->cancelButton.set_label("Cancel");
    this->cancelButton.set_halign(Gtk::Align::CENTER);
    this->cancelButton.signal_clicked().connect(
        sigc::mem_fun(*this, &ConnectingView::onCancelClicked)
    );

    // Assemble
    this->contentBox.append(this->spinner);
    this->contentBox.append(this->statusLabel);
    this->contentBox.append(this->cancelButton);

    this->append(this->contentBox);
}

void ConnectingView::setCancelCallback(CancelCallback callback) {
    this->cancelCallback = std::move(callback);
}

void ConnectingView::setServerAddress(const std::string& address) {
    this->statusLabel.set_text("Connecting to " + address + "...");
}

void ConnectingView::onCancelClicked() {
    if (this->cancelCallback) {
        this->cancelCallback();
    }
}
