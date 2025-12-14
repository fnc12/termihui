import Cocoa
import SnapKit

/// Root application controller managing navigation between screens
class ViewController: NSViewController {
    
    // MARK: - Child View Controllers
    private lazy var welcomeViewController = WelcomeViewController()
    private lazy var connectingViewController = ConnectingViewController()
    private lazy var terminalViewController = TerminalViewController()
    
    // MARK: - Properties  
    private let webSocketManager = WebSocketManager()
    private var currentState: AppState = .welcome {
        didSet {
            updateUIForState(currentState)
        }
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupDelegates()
        setupWindowObserver()
        determineInitialState()
    }
    
    override func viewDidAppear() {
        super.viewDidAppear()
        setupWindow()
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
        // Don't set fixed size - allow window to be resizable
        // Remove view.frame and preferredContentSize for flexibility
    }
    
    private func setupWindow() {
        guard let window = view.window else { return }
        
        // Make window resizable
        window.styleMask.insert(.resizable)
        
        // Add fullscreen support
        window.collectionBehavior = [.fullScreenPrimary]
        
        // Set initial and minimum sizes
        window.setContentSize(NSSize(width: 800, height: 600))
        window.minSize = NSSize(width: 400, height: 300)
        
        // Center window on screen
        window.center()
        
        // Manually manage ViewController.view size via frame
        if let contentView = window.contentView {
            view.translatesAutoresizingMaskIntoConstraints = true // Enable autoresizing
            view.frame = contentView.bounds
            print("🔧 Set initial ViewController frame: \(view.frame)")
        }
        
        print("🔧 Window configured: resizable, minimum 400x300, fullscreen support")
    }
    
    private func setupDelegates() {
        welcomeViewController.delegate = self
        connectingViewController.delegate = self
        terminalViewController.delegate = self
        webSocketManager.delegate = self
    }
    
    private func setupWindowObserver() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(windowDidResize(_:)),
            name: NSWindow.didResizeNotification,
            object: nil
        )
    }
    
    @objc private func windowDidResize(_ notification: Notification) {
        guard let window = notification.object as? NSWindow,
              window == view.window else { return }
        
        print("🔧 Window resized: \(window.frame.size)")
        
        // FORCE set ViewController frame to window size
        if let contentView = window.contentView {
            view.frame = contentView.bounds
            print("🔧 Set ViewController frame: \(view.frame)")
        }
        
        // Update child view controllers frame
        updateChildViewFrame()
        
        // Force update layout of all child controllers
        DispatchQueue.main.async {
            self.view.layoutSubtreeIfNeeded()
            self.children.forEach { child in
                child.view.layoutSubtreeIfNeeded()
            }
        }
    }
    
    private func updateChildViewFrame() {
        // Set frame of all child view controllers to parent view size
        children.forEach { child in
            child.view.frame = view.bounds
            print("🔧 Updated child controller frame: \(child.view.frame)")
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    private func determineInitialState() {
        // If saved address exists, try connecting immediately
        if AppSettings.shared.hasServerAddress {
            let serverAddress = AppSettings.shared.serverAddress
            currentState = .connecting(serverAddress: serverAddress)
            // Automatically initiate connection
            webSocketManager.connect(to: serverAddress)
        } else {
            currentState = .welcome
        }
    }
    
    // MARK: - Navigation Methods
    private func updateUIForState(_ state: AppState) {
        // Remove current child controller
        removeCurrentChildController()
        
        // Add new controller based on state
        switch state {
        case .welcome:
            showWelcomeScreen()
            
        case .connecting(let serverAddress):
            showConnectingScreen(serverAddress: serverAddress)
            
        case .connected(let serverAddress):
            showTerminalScreen(serverAddress: serverAddress)
            
        case .error(let message):
            showErrorAndReturnToWelcome(message: message)
        }
    }
    
    private func removeCurrentChildController() {
        // Remove all child controllers
        children.forEach { child in
            child.view.removeFromSuperview()
            child.removeFromParent()
        }
    }
    
    private func showWelcomeScreen() {
        addChild(welcomeViewController)
        view.addSubview(welcomeViewController.view)
        updateChildViewFrame()
    }
    
    private func showConnectingScreen(serverAddress: String) {
        connectingViewController.configure(serverAddress: serverAddress)
        addChild(connectingViewController)
        view.addSubview(connectingViewController.view)
        updateChildViewFrame()
        
        // Connection initiated in determineInitialState() or on button press in welcome
        // Here we only show UI
    }
    
    private func showTerminalScreen(serverAddress: String) {
        print("🔧 showTerminalScreen: Parent view size: \(view.frame)")
        print("🔧 showTerminalScreen: Window size: \(view.window?.frame.size ?? CGSize.zero)")
        
        terminalViewController.configure(serverAddress: serverAddress)
        terminalViewController.webSocketManager = webSocketManager
        addChild(terminalViewController)
        view.addSubview(terminalViewController.view)
        updateChildViewFrame()
        
        print("🔧 showTerminalScreen: Terminal view added with frame")
    }
    
    private func showErrorAndReturnToWelcome(message: String) {
        let alert = NSAlert()
        alert.messageText = "Ошибка подключения"
        alert.informativeText = message
        alert.alertStyle = .warning
        alert.addButton(withTitle: "OK")
        
        if let window = view.window {
            alert.beginSheetModal(for: window) { _ in
                self.currentState = .welcome
            }
        } else {
            alert.runModal()
            currentState = .welcome
        }
    }
}

// MARK: - WelcomeViewControllerDelegate
extension ViewController: WelcomeViewControllerDelegate {
    func welcomeViewController(_ controller: WelcomeViewController, didRequestConnectionTo serverAddress: String) {
        currentState = .connecting(serverAddress: serverAddress)
        // Initiate connection on manual address entry
        webSocketManager.connect(to: serverAddress)
    }
}

// MARK: - ConnectingViewControllerDelegate  
extension ViewController: ConnectingViewControllerDelegate {
    func connectingViewControllerDidCancel(_ controller: ConnectingViewController) {
        currentState = .welcome
    }
}

// MARK: - TerminalViewControllerDelegate
extension ViewController: TerminalViewControllerDelegate {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String) {
        // Send command through WebSocketManager
        webSocketManager.sendCommand(command)
    }
    
    func terminalViewControllerDidRequestDisconnect(_ controller: TerminalViewController) {
        webSocketManager.disconnect()
        currentState = .welcome
    }
}

// MARK: - WebSocketManagerDelegate
extension ViewController: WebSocketManagerDelegate {
    func webSocketManagerDidConnect(_ manager: WebSocketManager, initialCwd: String?) {
        DispatchQueue.main.async {
            if case .connecting(let serverAddress) = self.currentState {
                // First show terminal, then pass cwd
                self.currentState = .connected(serverAddress: serverAddress)
                // Now TerminalViewController is added — pass cwd and initial size
                if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                    if let cwd = initialCwd {
                        terminalVC.updateCurrentCwd(cwd)
                    }
                    // Send initial terminal size to server
                    terminalVC.sendInitialTerminalSize()
                }
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didFailWithError error: Error) {
        DispatchQueue.main.async {
            let errorMessage = error.localizedDescription
            self.currentState = .error(message: errorMessage)
        }
    }
    
    func webSocketManagerDidDisconnect(_ manager: WebSocketManager) {
        DispatchQueue.main.async {
            self.currentState = .welcome
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveOutput output: String) {
        print("🎯 ViewController received output: \(output)")
        // Handle server output in terminal
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                print("✅ Found TerminalViewController, calling appendOutput")
                terminalVC.appendOutput(output)
            } else {
                print("❌ TerminalViewController not found in children")
                print("🔍 Current children: \(self.children)")
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveStatus running: Bool, exitCode: Int) {
        // Handle process status changes
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                if !running && exitCode != 0 {
                    // Show error message only on unsuccessful completion
                    terminalVC.appendOutput("❌ Process exited with code \(exitCode)\n")
                }
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveCompletions completions: [String], originalText: String, cursorPosition: Int) {
        print("🎯 ViewController received completion options:")
        print("   Original text: '\(originalText)'")
        print("   Cursor position: \(cursorPosition)")
        print("   Options: \(completions)")
        
        // Pass completion options to TerminalViewController for handling
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                terminalVC.handleCompletionResults(completions, originalText: originalText, cursorPosition: cursorPosition)
            }
        }
    }

    // MARK: - Command events
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandStart command: String?, cwd: String?) {
        // Pass command and cwd as block header (if available)
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                if let cmd = command, !cmd.isEmpty {
                    terminalVC.didStartCommandBlock(command: cmd, cwd: cwd)
                } else {
                    terminalVC.didStartCommandBlock(cwd: cwd)
                }
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandEndWithExitCode exitCode: Int, cwd: String?) {
        // Notify TerminalViewController of block completion with cwd
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                terminalVC.didFinishCommandBlock(exitCode: exitCode, cwd: cwd)
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveHistory history: [CommandHistoryRecord]) {
        // Pass command history to TerminalViewController
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                terminalVC.loadHistory(history)
            }
        }
    }
}

