import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {

    /// Client core wrapper instance
    private var clientCore: ClientCoreWrapper?

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        print("🚀 AppDelegate: Starting TermiHUI application")
        print("📦 Client Core version: \(ClientCoreWrapper.version)")
        
        // Create and initialize C++ client core
        clientCore = ClientCoreWrapper()
        
        guard let core = clientCore, core.isValid else {
            print("❌ AppDelegate: Critical error - failed to create client core")
            NSApplication.shared.terminate(self)
            return
        }
        
        let initialized = core.initialize()
        if !initialized {
            print("❌ AppDelegate: Critical error - failed to initialize client core")
            NSApplication.shared.terminate(self)
            return
        }
        
        print("✅ AppDelegate: Application successfully started")
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Shutdown client core
        clientCore?.shutdown()
        clientCore = nil
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }
}
