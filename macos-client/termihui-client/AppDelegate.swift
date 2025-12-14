import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {

    


    func applicationDidFinishLaunching(_ aNotification: Notification) {
        print("🚀 AppDelegate: Starting TermiHUI application")
        
        // Initialize C++ client core
        let coreInitialized = ClientCoreWrapper.initializeApp()
        
        if !coreInitialized {
            print("❌ AppDelegate: Critical error - failed to initialize core")
            NSApplication.shared.terminate(self)
            return
        }
        
        print("✅ AppDelegate: Application successfully started")
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }


}

