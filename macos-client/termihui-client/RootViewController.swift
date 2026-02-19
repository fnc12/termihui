import Cocoa
import SnapKit

/// Root application controller managing navigation between screens
class RootViewController: NSViewController {
    
    // MARK: - Child View Controllers
    private lazy var welcomeViewController = WelcomeViewController()
    private lazy var connectingViewController = ConnectingViewController()
    private lazy var terminalViewController = TerminalViewController()
    
    // MARK: - Services
    private let messageDecoder = MessageDecoder()

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
        
        guard let data = event.data(using: .utf8) else {
            print("❌ Failed to convert event to data: \(event)")
            return
        }
        
        let json: [String: Any]
        do {
            guard let parsed = try JSONSerialization.jsonObject(with: data) as? [String: Any] else {
                print("❌ Event is not a JSON object: \(event)")
                return
            }
            json = parsed
        } catch {
            print("❌ Failed to parse event JSON: \(error.localizedDescription), event: \(event)")
            return
        }
        
        guard let type = json["type"] as? String else {
            print("❌ Event missing 'type' field: \(json)")
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
                // Request LLM providers list after connection
                clientCore?.send(["type": "list_llm_providers"])
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
            guard isActiveSession(messageDict) else { break }
            // New format: segments array from C++ ANSI parser
            if let segmentsData = messageDict["segments"] {
                do {
                    let jsonData = try JSONSerialization.data(withJSONObject: segmentsData)
                    let segments = try JSONDecoder().decode([StyledSegment].self, from: jsonData)
                    terminalViewController.appendStyledOutput(segments)
                } catch {
                    print("❌ Failed to decode segments: \(error)")
                }
            }
            // Fallback: raw data (for backward compatibility)
            else if let outputData = messageDict["data"] as? String {
                terminalViewController.appendOutput(outputData)
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
            guard isActiveSession(messageDict) else { break }
            let cwd = messageDict["cwd"] as? String
            let command = messageDict["command"] as? String
            terminalViewController.didStartCommandBlock(command: command, cwd: cwd)
            
        case "command_end":
            guard isActiveSession(messageDict) else { break }
            let exitCode = messageDict["exit_code"] as? Int ?? 0
            let cwd = messageDict["cwd"] as? String
            terminalViewController.didFinishCommandBlock(exitCode: exitCode, cwd: cwd)
            
        case "history":
            if let commands = messageDict["commands"] {
                switch messageDecoder.decode([CommandHistoryRecord].self, from: commands) {
                case .success(let records):
                    // print("📜 History: \(records.count) commands")
                    terminalViewController.loadHistory(records)
                case .failure(let error):
                    print("❌ [history] \(error.description)")
                }
            } else {
                print("❌ [history] missing 'commands' field")
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
            guard let sessionsData = messageDict["sessions"] else {
                print("❌ [sessions_list] missing 'sessions' field")
                break
            }
            switch messageDecoder.decode([SessionInfo].self, from: sessionsData) {
            case .success(let sessions):
                // print("📋 Sessions list: \(sessions.count) sessions")
                // Use active_session_id from client-core (restored from storage or first session)
                let activeSessionId = (messageDict["active_session_id"] as? UInt64)
                    ?? (messageDict["active_session_id"] as? Int).map { UInt64($0) }
                    ?? sessions.first?.id
                terminalViewController.updateSessionList(sessions, activeSessionId: activeSessionId)
                terminalViewController.updateSessionName(activeSessionId)
            case .failure(let error):
                print("❌ [sessions_list] \(error.description)")
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
            
        case "ai_chunk":
            if let content = messageDict["content"] as? String,
               let sessionId = messageDict["session_id"] as? UInt64 {
                print("🤖 AI chunk for session \(sessionId): \(content.prefix(30))...")
                terminalViewController.aiAppendChunk(content, forSession: sessionId)
            } else {
                print("❌ ai_chunk missing 'content' or 'session_id': \(messageDict)")
            }
            
        case "ai_done":
            if let sessionId = messageDict["session_id"] as? UInt64 {
                print("🤖 AI done for session \(sessionId)")
                terminalViewController.aiFinishResponse(forSession: sessionId)
            } else {
                print("❌ ai_done missing 'session_id': \(messageDict)")
            }
            
        case "ai_error":
            if let error = messageDict["error"] as? String,
               let sessionId = messageDict["session_id"] as? UInt64 {
                print("🤖 AI error for session \(sessionId): \(error)")
                terminalViewController.aiShowError(error, forSession: sessionId)
            } else {
                print("❌ ai_error missing 'error' or 'session_id': \(messageDict)")
            }
            
        case "llm_providers_list":
            guard let providersData = messageDict["providers"] else {
                print("❌ [llm_providers_list] missing 'providers' field")
                break
            }
            switch messageDecoder.decode([LLMProvider].self, from: providersData) {
            case .success(let providers):
                print("📋 Received \(providers.count) LLM providers")
                terminalViewController.updateLLMProviders(providers)
            case .failure(let error):
                print("❌ [llm_providers_list] \(error.description)")
            }
            
        case "llm_provider_added":
            if let providerId = messageDict["id"] as? UInt64 {
                print("✅ LLM provider added: \(providerId)")
                // Refresh providers list
                clientCore?.send(["type": "list_llm_providers"])
            }
            
        case "llm_provider_updated", "llm_provider_deleted":
            print("✅ LLM provider \(messageType)")
            // Refresh providers list
            clientCore?.send(["type": "list_llm_providers"])
            
        case "chat_history":
            guard let sessionId = messageDict["session_id"] as? UInt64 else {
                print("❌ [chat_history] missing 'session_id' field")
                break
            }
            guard let messagesData = messageDict["messages"] else {
                print("❌ [chat_history] missing 'messages' field")
                break
            }
            switch messageDecoder.decode([ChatMessageInfo].self, from: messagesData) {
            case .success(let messages):
                print("📜 Received chat history (\(messages.count) messages) for session \(sessionId)")
                terminalViewController.loadChatHistory(messages, forSession: sessionId)
            case .failure(let error):
                print("❌ [chat_history] \(error.description)")
            }
            
        // MARK: - Interactive Mode Messages
        case "block_screen_update":
            guard isActiveSession(messageDict) else { break }
            handleBlockScreenUpdate(messageDict)
            
        case "interactive_mode_start":
            let rows = messageDict["rows"] as? Int ?? 24
            let columns = messageDict["columns"] as? Int ?? 80
            print("🖥️ Interactive mode start: \(columns)x\(rows)")
            terminalViewController.enterInteractiveMode(rows: rows, columns: columns)
            
        case "screen_snapshot":
            print("🖥️ Screen snapshot received")
            handleScreenSnapshot(messageDict)
            
        case "screen_diff":
            handleScreenDiff(messageDict)
            
        case "interactive_mode_end":
            print("🖥️ Interactive mode end")
            terminalViewController.exitInteractiveMode()
            
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
    
    // MARK: - Interactive Mode Helpers
    
    private func handleScreenSnapshot(_ messageDict: [String: Any]) {
        guard let linesData = messageDict["lines"],
              let cursorRow = messageDict["cursor_row"] as? Int,
              let cursorColumn = messageDict["cursor_column"] as? Int else {
            print("❌ screen_snapshot missing fields: \(messageDict)")
            return
        }
        
        do {
            let jsonData = try JSONSerialization.data(withJSONObject: linesData)
            let lines = try JSONDecoder().decode([[StyledSegment]].self, from: jsonData)
            terminalViewController.handleScreenSnapshot(lines: lines, cursorRow: cursorRow, cursorColumn: cursorColumn)
        } catch {
            print("❌ Failed to decode screen_snapshot lines: \(error)")
        }
    }
    
    private func isActiveSession(_ messageDict: [String: Any]) -> Bool {
        guard let sessionId = messageDict["session_id"] as? UInt64
                ?? (messageDict["session_id"] as? Int).map({ UInt64($0) }) else {
            return true // No session_id → legacy message, allow through
        }
        return sessionId == terminalViewController.cachedActiveSessionId
    }
    
    private func handleBlockScreenUpdate(_ messageDict: [String: Any]) {
        guard let updatesData = messageDict["updates"],
              let cursorRow = messageDict["cursor_row"] as? Int,
              let cursorColumn = messageDict["cursor_column"] as? Int else {
            print("❌ block_screen_update missing fields")
            return
        }
        
        do {
            let jsonData = try JSONSerialization.data(withJSONObject: updatesData)
            let updates = try JSONDecoder().decode([ScreenRowUpdate].self, from: jsonData)
            terminalViewController.handleBlockScreenUpdate(updates: updates, cursorRow: cursorRow, cursorColumn: cursorColumn)
        } catch {
            print("❌ Failed to decode block_screen_update: \(error)")
        }
    }
    
    private func handleScreenDiff(_ messageDict: [String: Any]) {
        guard let updatesData = messageDict["updates"],
              let cursorRow = messageDict["cursor_row"] as? Int,
              let cursorColumn = messageDict["cursor_column"] as? Int else {
            print("❌ screen_diff missing fields: \(messageDict)")
            return
        }
        
        do {
            let jsonData = try JSONSerialization.data(withJSONObject: updatesData)
            let updates = try JSONDecoder().decode([ScreenRowUpdate].self, from: jsonData)
            terminalViewController.handleScreenDiff(updates: updates, cursorRow: cursorRow, cursorColumn: cursorColumn)
        } catch {
            print("❌ Failed to decode screen_diff updates: \(error)")
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
        // Reset window title
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
        
        // Set window title
        view.window?.title = "TermiHUI — \(serverAddress)"
        
        terminalViewController.configure(serverAddress: serverAddress)
        addChild(terminalViewController)
        view.addSubview(terminalViewController.view)
        updateChildViewFrame()
        
        print("🔧 showTerminalScreen: Terminal view added with frame")
    }
    
    private func showErrorAndReturnToWelcome(message: String) {
        let alert = NSAlert()
        alert.messageText = "Connection Error"
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

// MARK: - Public Methods (for menu)
extension RootViewController {
    /// Called from AppDelegate when Client -> Disconnect is clicked
    func requestDisconnect() {
        // Clear terminal state before disconnecting
        terminalViewController.clearState()
        clientCore?.send(["type": "disconnectButtonClicked"])
    }
}
