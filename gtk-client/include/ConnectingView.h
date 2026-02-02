#pragma once

#include <gtkmm.h>
#include <functional>

/// Connecting screen with spinner (like ConnectingViewController)
class ConnectingView : public Gtk::Box {
public:
    using CancelCallback = std::function<void()>;

    ConnectingView();

    void setCancelCallback(CancelCallback callback);

    /// Update displayed server address
    void setServerAddress(const std::string& address);

private:
    void onCancelClicked();

    CancelCallback cancelCallback;

    Gtk::Box contentBox;
    Gtk::Spinner spinner;
    Gtk::Label statusLabel;
    Gtk::Button cancelButton;
};
