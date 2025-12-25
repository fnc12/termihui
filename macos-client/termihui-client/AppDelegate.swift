import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {

    /// Client core wrapper instance (accessible for reading by other components)
    private(set) var clientCore: ClientCoreWrapper?
    
    /// Disconnect menu item (to enable/disable based on connection state)
    private var disconnectMenuItem: NSMenuItem?
    
    /// Flag to track if clientCore was already passed to ViewController
    private var clientCorePassedToVC = false

    func applicationWillFinishLaunching(_ notification: Notification) {
        // Initialize client core BEFORE window loads
        print("🚀 AppDelegate: Initializing client core (before window)")
        print("📦 Client Core version: \(ClientCoreWrapper.version)")
        
        clientCore = ClientCoreWrapper()
        
        guard let core = clientCore else {
            print("❌ AppDelegate: Critical error - failed to create client core")
            return
        }
        
        let initialized = core.initialize()
        if !initialized {
            print("❌ AppDelegate: Critical error - failed to initialize client core")
        }
    }
    
    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Setup Client menu
        setupClientMenu()
        
        // Pass clientCore to main ViewController (window should be ready now)
        passClientCoreToViewController()
        
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

    func applicationDidBecomeActive(_ notification: Notification) {
        // Fallback: pass clientCore if window wasn't ready during launch
        passClientCoreToViewController()
    }
    
    // MARK: - Client Core Injection
    
    private func passClientCoreToViewController() {
        guard !clientCorePassedToVC, let core = clientCore else { return }
        
        // Try multiple ways to find the window
        let window = NSApplication.shared.mainWindow 
            ?? NSApplication.shared.keyWindow 
            ?? NSApplication.shared.windows.first
        
        if let viewController = window?.contentViewController as? ViewController {
            print("📲 Passing clientCore to ViewController")
            viewController.clientCore = core
            clientCorePassedToVC = true
        } else {
            print("⚠️ Could not find ViewController, will retry later")
        }
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
