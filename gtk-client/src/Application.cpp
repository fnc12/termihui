#include "Application.h"
#include "MainWindow.h"
#include "ClientCoreWrapper.h"
#include <fmt/core.h>

Glib::RefPtr<Application> Application::create() {
    return Glib::make_refptr_for_instance<Application>(new Application());
}

Application::Application()
    : Gtk::Application("com.termihui.gtkclient") {

    fmt::print("[Application] Creating...\n");
    fmt::print("[Application] Client core version: {}\n", ClientCoreWrapper::getVersion());

    this->clientCore = std::make_unique<ClientCoreWrapper>();
}

void Application::on_startup() {
    Gtk::Application::on_startup();

    // Load CSS
    auto cssProvider = Gtk::CssProvider::create();
    try {
        cssProvider->load_from_path("../resources/style.css");
    } catch (const Glib::Error& e) {
        // Try alternate path (when running from different directory)
        try {
            cssProvider->load_from_path("resources/style.css");
        } catch (const Glib::Error& e2) {
            fmt::print(stderr, "[Application] Failed to load CSS: {}\n", e2.what());
        }
    }

    Gtk::StyleContext::add_provider_for_display(
        Gdk::Display::get_default(),
        cssProvider,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

Application::~Application() {
    fmt::print("[Application] Destroying...\n");
}

void Application::on_activate() {
    fmt::print("[Application] Activating...\n");

    // Initialize client core
    if (!this->clientCore->isInitialized()) {
        if (!this->clientCore->initialize()) {
            fmt::print(stderr, "[Application] Failed to initialize client core\n");
        }
    }

    // Create main window with reference to client core
    auto window = new MainWindow(*this->clientCore);
    this->add_window(*window);
    window->present();
}
