import Cocoa
import SnapKit

/// Root application controller managing navigation between screens
class RootViewController: NSViewController {
    
    // MARK: - Child View Controllers
    private lazy var welcomeViewController = WelcomeViewController()
    private lazy var connectingViewController = ConnectingViewController()
    private lazy var terminalViewController = TerminalViewController()
    
    // MARK: - Properties  
    
    /// Client core instance, passed from AppDelegate
    var clientCore: ClientCoreWrapper? {
        didSet {
            // Propagate to child controllers
            welcomeViewController.clientCore = clientCore
            connectingViewController.clientCore = clientCore
            terminalViewController.clientCore = clientCore
            
            // Try auto-connect if not done yet
            if clientCore != nil && !initialStateDetermined {
                determineInitialState()
            }
        }
    }
    
    private var pollTimer: Timer?
    private var initialStateDetermined = false
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
        startPollTimer()
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
        pollTimer?.invalidate()
        NotificationCenter.default.removeObserver(self)
    }
    
    // MARK: - Poll Timer
    
    private func startPollTimer() {
        // Poll events every 16ms (~60fps)
        pollTimer = Timer.scheduledTimer(withTimeInterval: 0.016, repeats: true) { [weak self] _ in
            self?.pollEvents()
        }
    }
    
    private func pollEvents() {
        guard let clientCore = clientCore else { return }
        
        // Process WebSocket events on main thread
        clientCore.update()
        
        while let event = clientCore.pollEvent() {
            handleEvent(event)
        }
    }
    
    private func handleEvent(_ event: String) {
        // print("📥 ClientCore event: \(event)")
        
        guard let data = event.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = json["type"] as? String else {
            print("❌ Failed to parse event JSON: \(event)")
            return
        }
        
        switch type {
        case "connectionStateChanged":
            if let state = json["state"] as? String {
                handleConnectionStateChanged(state: state, json: json)
            } else {
                print("❌ connectionStateChanged missing 'state': \(json)")
            }
        case "serverMessage":
            if let messageData = json["data"] {
                handleServerMessage(messageData)
            } else {
                print("❌ serverMessage missing 'data': \(json)")
            }
        case "error":
            if let message = json["message"] as? String {
                print("❌ ClientCore error: \(message)")
            } else {
                print("❌ error missing 'message': \(json)")
            }
        default:
            print("⚠️ Unknown event type: '\(type)', json: \(json)")
        }
    }
    
    private func handleConnectionStateChanged(state: String, json: [String: Any]) {
        print("🔌 Connection state: \(state)")
        
        switch state {
        case "connecting":
            if let address = json["address"] as? String {
                currentState = .connecting(serverAddress: address)
            } else {
                print("❌ connecting missing 'address': \(json)")
            }
        case "connected":
            if let address = json["address"] as? String {
                currentState = .connected(serverAddress: address)
            } else {
                print("❌ connected missing 'address': \(json)")
            }
        case "disconnected":
            currentState = .welcome
        default:
            print("⚠️ Unknown connection state: '\(state)', json: \(json)")
        }
    }
    
    private func handleServerMessage(_ data: Any) {
        guard let messageDict = data as? [String: Any],
              let messageType = messageDict["type"] as? String else {
            print("❌ Invalid server message format (not dict or missing 'type'): \(data)")
            return
        }
        
        switch messageType {
        case "connected":
            if let cwd = messageDict["cwd"] as? String {
                print("📂 Initial CWD: \(cwd)")
                terminalViewController.updateCurrentCwd(cwd)
            } else {
                print("⚠️ connected missing 'cwd': \(messageDict)")
            }
            if let home = messageDict["home"] as? String {
                print("🏠 Server home: \(home)")
                terminalViewController.updateServerHome(home)
            }
            
        case "output":
            // New format: segments array from C++ ANSI parser
            if let segmentsData = messageDict["segments"] {
                do {
                    let jsonData = try JSONSerialization.data(withJSONObject: segmentsData)
                    let segments = try JSONDecoder().decode([StyledSegment].self, from: jsonData)
                    print("📺 Output: \(segments.count) segments")
                    terminalViewController.appendStyledOutput(segments)
                } catch {
                    print("❌ Failed to decode segments: \(error)")
                }
            }
            // Fallback: raw data (for backward compatibility)
            else if let outputData = messageDict["data"] as? String {
                print("📺 Output (raw): \(outputData.prefix(50))...")
                terminalViewController.appendOutput(outputData)
            } else {
                print("❌ output missing 'segments' or 'data': \(messageDict)")
            }
            
        case "status":
            if let running = messageDict["running"] as? Bool,
               let exitCode = messageDict["exit_code"] as? Int {
                print("📊 Status: running=\(running), exitCode=\(exitCode)")
                if !running && exitCode != 0 {
                    terminalViewController.appendOutput("\n[Exit code: \(exitCode)]\n")
                }
            } else {
                print("❌ status missing 'running' or 'exit_code': \(messageDict)")
            }
            
        case "command_start":
            let cwd = messageDict["cwd"] as? String
            let command = messageDict["command"] as? String
            print("▶️ Command start: '\(command ?? "nil")', cwd=\(cwd ?? "nil")")
            terminalViewController.didStartCommandBlock(command: command, cwd: cwd)
            
        case "command_end":
            let exitCode = messageDict["exit_code"] as? Int ?? 0
            let cwd = messageDict["cwd"] as? String
            print("🏁 Command end, exitCode=\(exitCode), cwd=\(cwd ?? "nil")")
            terminalViewController.didFinishCommandBlock(exitCode: exitCode, cwd: cwd)
            
        case "history":
            if let commands = messageDict["commands"] {
                if let commandsData = try? JSONSerialization.data(withJSONObject: commands),
                   let records = try? JSONDecoder().decode([CommandHistoryRecord].self, from: commandsData) {
                    // print("📜 History: \(records.count) commands")
                    terminalViewController.loadHistory(records)
                } else {
                    print("❌ history failed to decode 'commands': \(commands)")
                }
            } else {
                print("❌ history missing 'commands': \(messageDict)")
            }
            
        case "cwd_update":
            if let cwd = messageDict["cwd"] as? String {
                print("📂 CWD update: \(cwd)")
                terminalViewController.updateCurrentCwd(cwd)
            } else {
                print("❌ cwd_update missing 'cwd': \(messageDict)")
            }
            
        case "completion_result":
            if let completions = messageDict["completions"] as? [String],
               let originalText = messageDict["original_text"] as? String,
               let cursorPosition = messageDict["cursor_position"] as? Int {
                print("🔤 Completions: \(completions.count) options")
                terminalViewController.handleCompletionResults(completions, originalText: originalText, cursorPosition: cursorPosition)
            } else {
                print("❌ completion_result missing fields: \(messageDict)")
            }
            
        case "sessions_list":
            if let sessionsData = messageDict["sessions"],
               let jsonData = try? JSONSerialization.data(withJSONObject: sessionsData),
               let sessions = try? JSONDecoder().decode([SessionInfo].self, from: jsonData) {
                // print("📋 Sessions list: \(sessions.count) sessions")
                // Use active_session_id from client-core (restored from storage or first session)
                let activeSessionId = (messageDict["active_session_id"] as? UInt64)
                    ?? (messageDict["active_session_id"] as? Int).map { UInt64($0) }
                    ?? sessions.first?.id
                terminalViewController.updateSessionList(sessions, activeSessionId: activeSessionId)
                terminalViewController.updateSessionName(activeSessionId)
            } else {
                print("❌ sessions_list failed to decode: \(messageDict)")
            }
            
        case "session_created":
            if let sessionId = messageDict["session_id"] as? UInt64 ?? (messageDict["session_id"] as? Int).map({ UInt64($0) }) {
                print("✅ Session created: #\(sessionId)")
                terminalViewController.setActiveSession(sessionId)
                terminalViewController.updateSessionName(sessionId)
            } else {
                print("❌ session_created missing 'session_id': \(messageDict)")
            }
            
        case "session_closed":
            if let sessionId = messageDict["session_id"] as? UInt64 ?? (messageDict["session_id"] as? Int).map({ UInt64($0) }) {
                print("🗑️ Session closed: #\(sessionId)")
                // UI will be updated when new sessions_list arrives
            } else {
                print("❌ session_closed missing 'session_id': \(messageDict)")
            }
            
        case "input_sent", "resize_ack":
            // Acknowledgements - can be ignored
            break
            
        case "error":
            if let message = messageDict["message"] as? String {
                print("❌ Server error: \(message)")
                showErrorAlert(message: message)
            } else {
                print("❌ error missing 'message': \(messageDict)")
            }
            
        default:
            print("⚠️ Unknown server message type: '\(messageType)', data: \(messageDict)")
        }
    }
    
    private func determineInitialState() {
        guard !initialStateDetermined else { return }
        guard clientCore != nil else { return } // Need clientCore to proceed
        
        initialStateDetermined = true
        
        // If saved address exists, try connecting immediately
        if AppSettings.shared.hasServerAddress {
            let serverAddress = AppSettings.shared.serverAddress
            print("🔌 Auto-reconnecting to: \(serverAddress)")
            // Send reconnect request to core
            clientCore?.send(["type": "requestReconnect", "address": serverAddress])
        } else {
            currentState = .welcome
        }
    }
    
    // MARK: - Navigation Methods
    private func updateUIForState(_ state: AppState) {
        // Remove current child controller
        removeCurrentChildController()
        
        // Notify about connection state change
        let isConnected: Bool
        switch state {
        case .connected:
            isConnected = true
        default:
            isConnected = false
        }
        NotificationCenter.default.post(
            name: .connectionStateChanged,
            object: nil,
            userInfo: ["isConnected": isConnected]
        )
        
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
        // Сбрасываем заголовок окна
        view.window?.title = "TermiHUI"
        
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
        
        // Устанавливаем заголовок окна
        view.window?.title = "TermiHUI — \(serverAddress)"
        
        terminalViewController.configure(serverAddress: serverAddress)
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
extension RootViewController: WelcomeViewControllerDelegate {
    func welcomeViewController(_ controller: WelcomeViewController, didClickConnectButton serverAddress: String) {
        // Send event to core - it will handle connection and state updates
        clientCore?.send(["type": "connectButtonClicked", "address": serverAddress])
    }
}

// MARK: - ConnectingViewControllerDelegate  
extension RootViewController: ConnectingViewControllerDelegate {
    func connectingViewControllerDidCancel(_ controller: ConnectingViewController) {
        currentState = .welcome
    }
}

// MARK: - TerminalViewControllerDelegate
extension RootViewController: TerminalViewControllerDelegate {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String) {
        // Send command through ClientCore (it saves command for block header)
        clientCore?.send(["type": "executeCommand", "command": command])
    }
    
    func terminalViewControllerDidRequestDisconnect(_ controller: TerminalViewController) {
        controller.clearState()
        clientCore?.send(["type": "disconnectButtonClicked"])
    }
    
    private func showErrorAlert(message: String) {
        let alert = NSAlert()
        alert.messageText = "Server Error"
        alert.informativeText = message
        alert.alertStyle = .warning
        alert.addButton(withTitle: "OK")
        alert.runModal()
    }
}

// MARK: - Public Methods (для меню)
extension RootViewController {
    /// Вызывается из AppDelegate при нажатии Client -> Disconnect
    func requestDisconnect() {
        // Очищаем состояние терминала перед отключением
        terminalViewController.clearState()
        clientCore?.send(["type": "disconnectButtonClicked"])
    }
}
