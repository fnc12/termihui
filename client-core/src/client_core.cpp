#include "termihui/client_core.h"
#include "termihui/client_core_c.h"
#include <fmt/core.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>

namespace termihui {

// Version
static constexpr const char* VERSION = "1.0.0";

// Handle storage
static std::mutex handlesMutex;
static std::unordered_map<int, std::unique_ptr<ClientCore>> handles;
static std::atomic<int> nextHandle{1};

// ClientCore implementation

ClientCore::ClientCore() = default;

ClientCore::~ClientCore() {
    if (initialized) {
        shutdown();
    }
}

const char* ClientCore::getVersion() {
    return VERSION;
}

bool ClientCore::initialize() {
    if (initialized) {
        fmt::print(stderr, "ClientCore: Already initialized\n");
        return false;
    }
    
    fmt::print("ClientCore: Initializing v{}\n", VERSION);
    initialized = true;
    fmt::print("ClientCore: Initialized successfully\n");
    
    return true;
}

void ClientCore::shutdown() {
    if (!initialized) {
        return;
    }
    
    fmt::print("ClientCore: Shutting down\n");
    initialized = false;
}

// C++ API (namespace functions)

int createClientCore() {
    std::lock_guard<std::mutex> lock(handlesMutex);
    
    int handle = nextHandle.fetch_add(1);
    handles[handle] = std::make_unique<ClientCore>();
    
    fmt::print("ClientCore: Created instance with handle {}\n", handle);
    return handle;
}

void destroyClientCore(int handle) {
    std::lock_guard<std::mutex> lock(handlesMutex);
    
    auto it = handles.find(handle);
    if (it != handles.end()) {
        handles.erase(it);
        fmt::print("ClientCore: Destroyed instance with handle {}\n", handle);
    } else {
        fmt::print(stderr, "ClientCore: Invalid handle {} in destroyClientCore\n", handle);
    }
}

bool initializeClientCore(int handle) {
    std::lock_guard<std::mutex> lock(handlesMutex);
    
    auto it = handles.find(handle);
    if (it != handles.end()) {
        return it->second->initialize();
    }
    
    fmt::print(stderr, "ClientCore: Invalid handle {} in initializeClientCore\n", handle);
    return false;
}

void shutdownClientCore(int handle) {
    std::lock_guard<std::mutex> lock(handlesMutex);
    
    auto it = handles.find(handle);
    if (it != handles.end()) {
        it->second->shutdown();
    } else {
        fmt::print(stderr, "ClientCore: Invalid handle {} in shutdownClientCore\n", handle);
    }
}

bool isValidClientCore(int handle) {
    std::lock_guard<std::mutex> lock(handlesMutex);
    return handles.find(handle) != handles.end();
}

const char* getClientCoreVersion() {
    return VERSION;
}

} // namespace termihui

// C API implementation (extern "C")

extern "C" {

int termihui_create_client(void) {
    return termihui::createClientCore();
}

void termihui_destroy_client(int handle) {
    termihui::destroyClientCore(handle);
}

bool termihui_initialize_client(int handle) {
    return termihui::initializeClientCore(handle);
}

void termihui_shutdown_client(int handle) {
    termihui::shutdownClientCore(handle);
}

bool termihui_is_valid_client(int handle) {
    return termihui::isValidClientCore(handle);
}

const char* termihui_get_version(void) {
    return termihui::getClientCoreVersion();
}

}
