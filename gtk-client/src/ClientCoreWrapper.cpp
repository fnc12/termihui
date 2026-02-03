#include "ClientCoreWrapper.h"
#include <termihui/client_core_c.h>
#include <fmt/core.h>

ClientCoreWrapper::ClientCoreWrapper() {
    fmt::print("[ClientCoreWrapper] Created\n");
}

ClientCoreWrapper::~ClientCoreWrapper() {
    if (this->isInitialized()) {
        this->shutdown();
    }
}

std::string ClientCoreWrapper::getVersion() {
    const char* version = termihui_get_version();
    return version ? version : "unknown";
}

bool ClientCoreWrapper::initialize() {
    fmt::print("[ClientCoreWrapper] Initializing...\n");
    bool result = termihui_initialize();

    if (result) {
        fmt::print("[ClientCoreWrapper] Initialized successfully\n");
    } else {
        fmt::print(stderr, "[ClientCoreWrapper] Initialization failed\n");
    }

    return result;
}

void ClientCoreWrapper::shutdown() {
    fmt::print("[ClientCoreWrapper] Shutting down...\n");
    termihui_shutdown();
}

bool ClientCoreWrapper::isInitialized() const {
    return termihui_is_initialized();
}

std::string ClientCoreWrapper::sendMessage(const std::string& message) {
    const char* response = termihui_send_message(message.c_str());
    return response ? response : R"({"error":"null_response"})";
}

void ClientCoreWrapper::update() {
    termihui_update();
}

std::string ClientCoreWrapper::pollEvent() {
    const char* event = termihui_poll_event();
    return event ? event : "";
}

int ClientCoreWrapper::pendingEventsCount() const {
    return termihui_pending_events_count();
}
