import Foundation

/// Swift wrapper for TermiHUI C++ client core
class ClientCoreWrapper {
    
    /// Initializes and starts TermiHUI client core
    /// @return true if successfully started, false on error
    static func initializeApp() -> Bool {
        print("🔗 ClientCoreWrapper: Initializing C++ core...")
        let result = termihui_create_app()
        
        if result {
            print("✅ ClientCoreWrapper: C++ core successfully initialized")
        } else {
            print("❌ ClientCoreWrapper: C++ core initialization error")
        }
        
        return result
    }
}
