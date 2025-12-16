#pragma once

#include <string>

namespace termihui {

/**
 * TermiHUI Client Core
 * Manages connection state and protocol handling
 */
class ClientCore {
public:
    ClientCore();
    ~ClientCore();
    
    // Disable copying
    ClientCore(const ClientCore&) = delete;
    ClientCore& operator=(const ClientCore&) = delete;
    
    /**
     * Get client core version
     * @return version string
     */
    static const char* getVersion();
    
    /**
     * Initialize client core
     * @return true if successful
     */
    bool initialize();
    
    /**
     * Shutdown client core
     */
    void shutdown();
    
    /**
     * Check if initialized
     */
    bool isInitialized() const { return initialized; }

private:
    bool initialized = false;
};

// Handle-based C API (for FFI bindings)
// Use int handles instead of pointers for easier interop with C#, Kotlin, etc.

/**
 * Create new client core instance
 * @return handle (positive int) or -1 on error
 */
int createClientCore();

/**
 * Destroy client core instance
 * @param handle client core handle
 */
void destroyClientCore(int handle);

/**
 * Initialize client core
 * @param handle client core handle
 * @return true if successful
 */
bool initializeClientCore(int handle);

/**
 * Shutdown client core
 * @param handle client core handle
 */
void shutdownClientCore(int handle);

/**
 * Check if client core is valid handle
 * @param handle client core handle
 * @return true if handle is valid
 */
bool isValidClientCore(int handle);

/**
 * Get library version
 * @return version string (static, do not free)
 */
const char* getClientCoreVersion();

} // namespace termihui
