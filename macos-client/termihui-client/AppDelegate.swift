import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {

    /// Client core wrapper instance
    private var clientCore: ClientCoreWrapper?
    
    /// Disconnect menu item (to enable/disable based on connection state)
    private var disconnectMenuItem: NSMenuItem?

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
        
        // Setup Client menu
        setupClientMenu()
        
        // Subscribe to connection state notifications
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(connectionStateChanged(_:)),
            name: .connectionStateChanged,
            object: nil
        )
        
        print("✅ AppDelegate: Application successfully started")
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Shutdown client core
        clientCore?.shutdown()
        clientCore = nil
        
        NotificationCenter.default.removeObserver(self)
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }
    
    // MARK: - Menu Setup
    
    private func setupClientMenu() {
        guard let mainMenu = NSApplication.shared.mainMenu else { return }
        
        // Create "Client" menu
        let clientMenu = NSMenu(title: "Client")
        
        // Disconnect item
        let disconnectItem = NSMenuItem(
            title: "Disconnect",
            action: #selector(disconnectAction(_:)),
            keyEquivalent: "d"
        )
        disconnectItem.keyEquivalentModifierMask = [.command, .shift]
        disconnectItem.target = self
        disconnectItem.isEnabled = false // Initially disabled
        clientMenu.addItem(disconnectItem)
        
        self.disconnectMenuItem = disconnectItem
        
        // Create menu item for main menu bar
        let clientMenuItem = NSMenuItem()
        clientMenuItem.submenu = clientMenu
        
        // Insert after "File" menu (index 1)
        if mainMenu.items.count > 1 {
            mainMenu.insertItem(clientMenuItem, at: 2)
        } else {
            mainMenu.addItem(clientMenuItem)
        }
    }
    
    // MARK: - Menu Actions
    
    @objc private func disconnectAction(_ sender: Any?) {
        // Find ViewController and request disconnect
        if let window = NSApplication.shared.mainWindow,
           let viewController = window.contentViewController as? ViewController {
            viewController.requestDisconnect()
        }
    }
    
    // MARK: - Connection State
    
    @objc private func connectionStateChanged(_ notification: Notification) {
        guard let isConnected = notification.userInfo?["isConnected"] as? Bool else { return }
        
        DispatchQueue.main.async {
            self.disconnectMenuItem?.isEnabled = isConnected
        }
    }
}

// MARK: - Notification Names

extension Notification.Name {
    static let connectionStateChanged = Notification.Name("connectionStateChanged")
}
