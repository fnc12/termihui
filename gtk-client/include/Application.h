#pragma once

#include <gtkmm.h>
#include <memory>

class ClientCoreWrapper;

class Application : public Gtk::Application {
public:
    static Glib::RefPtr<Application> create();

protected:
    Application();
    ~Application() override;

    void on_startup() override;
    void on_activate() override;

private:
    std::unique_ptr<ClientCoreWrapper> clientCore;
};
