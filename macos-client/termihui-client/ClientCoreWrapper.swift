import Foundation

/// Swift wrapper for TermiHUI C++ client core
/// Uses handle-based API for cross-platform compatibility
class ClientCoreWrapper {
    
    /// Invalid handle constant
    static let invalidHandle: Int32 = -1
    
    /// Client core handle
    private var handle: Int32 = invalidHandle
    
    /// Check if wrapper has valid handle
    var isValid: Bool {
        return handle != ClientCoreWrapper.invalidHandle && termihui_is_valid_client(handle)
    }
    
    /// Get client core version
    static var version: String {
        guard let cString = termihui_get_version() else {
            return "unknown"
        }
        return String(cString: cString)
    }
    
    init() {
        print("🔗 ClientCoreWrapper: Creating C++ core instance...")
        handle = termihui_create_client()
        
        if handle != ClientCoreWrapper.invalidHandle {
            print("✅ ClientCoreWrapper: Created with handle \(handle)")
        } else {
            print("❌ ClientCoreWrapper: Failed to create instance")
        }
    }
    
    deinit {
        if handle != ClientCoreWrapper.invalidHandle {
            print("🔗 ClientCoreWrapper: Destroying handle \(handle)")
            termihui_destroy_client(handle)
        }
    }
    
    /// Initialize client core
    /// @return true if successful
    func initialize() -> Bool {
        guard handle != ClientCoreWrapper.invalidHandle else {
            print("❌ ClientCoreWrapper: Cannot initialize - invalid handle")
            return false
        }
        
        print("🔗 ClientCoreWrapper: Initializing...")
        let result = termihui_initialize_client(handle)
        
        if result {
            print("✅ ClientCoreWrapper: Initialized successfully")
        } else {
            print("❌ ClientCoreWrapper: Initialization failed")
        }
        
        return result
    }
    
    /// Shutdown client core
    func shutdown() {
        guard handle != ClientCoreWrapper.invalidHandle else {
            return
        }
        
        print("🔗 ClientCoreWrapper: Shutting down...")
        termihui_shutdown_client(handle)
    }
}
