#include "WelcomeView.h"
#include <gdk/gdkkeysyms.h>

WelcomeView::WelcomeView() {
    this->set_orientation(Gtk::Orientation::VERTICAL);
    this->set_valign(Gtk::Align::CENTER);
    this->set_halign(Gtk::Align::CENTER);

    // Content box
    this->contentBox.set_orientation(Gtk::Orientation::VERTICAL);
    this->contentBox.set_spacing(16);
    this->contentBox.set_margin(40);

    // Title
    this->titleLabel.set_markup("<span size='xx-large' weight='bold'>TermiHUI</span>");
    this->titleLabel.set_halign(Gtk::Align::CENTER);

    // Subtitle
    this->subtitleLabel.set_text("Enter server address to connect");
    this->subtitleLabel.set_halign(Gtk::Align::CENTER);
    this->subtitleLabel.add_css_class("dim-label");

    // Address entry
    this->addressEntry.set_text("localhost:37854");
    this->addressEntry.set_placeholder_text("host:port");
    this->addressEntry.set_halign(Gtk::Align::CENTER);
    this->addressEntry.set_size_request(250, -1);

    // Handle Enter key
    auto keyController = Gtk::EventControllerKey::create();
    keyController->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) -> bool {
            if (keyval == GDK_KEY_Return || keyval == GDK_KEY_KP_Enter) {
                this->onConnectClicked();
                return true;
            }
            return false;
        },
        false
    );
    this->addressEntry.add_controller(keyController);

    // Connect button
    this->connectButton.set_label("Connect");
    this->connectButton.set_halign(Gtk::Align::CENTER);
    this->connectButton.add_css_class("suggested-action");
    this->connectButton.signal_clicked().connect(
        sigc::mem_fun(*this, &WelcomeView::onConnectClicked)
    );

    // Assemble
    this->contentBox.append(this->titleLabel);
    this->contentBox.append(this->subtitleLabel);
    this->contentBox.append(this->addressEntry);
    this->contentBox.append(this->connectButton);

    this->append(this->contentBox);
}

void WelcomeView::setConnectCallback(ConnectCallback callback) {
    this->connectCallback = std::move(callback);
}

void WelcomeView::setAddress(const std::string& address) {
    this->addressEntry.set_text(address);
}

std::string WelcomeView::getAddress() const {
    return this->addressEntry.get_text();
}

void WelcomeView::onConnectClicked() {
    if (this->connectCallback) {
        this->connectCallback(this->addressEntry.get_text());
    }
}

