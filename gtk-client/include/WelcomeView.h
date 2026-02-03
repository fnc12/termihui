#pragma once

#include <gtkmm.h>
#include <functional>

/// Welcome screen with server address input (like WelcomeViewController)
class WelcomeView : public Gtk::Box {
public:
    using ConnectCallback = std::function<void(const std::string& address)>;

    WelcomeView();

    void setConnectCallback(ConnectCallback callback);

    /// Set saved server address
    void setAddress(const std::string& address);

    /// Get current address
    std::string getAddress() const;

private:
    void onConnectClicked();

    ConnectCallback connectCallback;

    Gtk::Box contentBox;
    Gtk::Label titleLabel;
    Gtk::Label subtitleLabel;
    Gtk::Entry addressEntry;
    Gtk::Button connectButton;
};
