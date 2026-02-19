import UIKit

class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?
    
    // MARK: - Core
    var clientCore: ClientCoreWrapper?
    private var pollTimer: Timer?
    
    // MARK: - State
    var isConnected = false
    var isUserInitiatedDisconnect = false
    var currentServerAddress: String?
    
    // MARK: - View Controllers
    var navigationController: UINavigationController?
    private var serverListVC: ServerListViewController?
    var sessionListVC: SessionListViewController?
    var terminalVC: TerminalViewController?
    var _chatVC: ChatViewController?  // Cached chat VC for session
    var _llmProvidersVC: LLMProvidersViewController?  // Currently open providers VC

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        guard let windowScene = (scene as? UIWindowScene) else { return }
        
        // Initialize ClientCore
        clientCore = ClientCoreWrapper()
        if clientCore?.initialize() == true {
            print("✅ ClientCore initialized, version: \(ClientCoreWrapper.version)")
        } else {
            print("❌ Failed to initialize ClientCore")
        }
        
        // Create UI
        let serverListVC = ServerListViewController()
        serverListVC.delegate = self
        self.serverListVC = serverListVC
        
        let navController = UINavigationController(rootViewController: serverListVC)
        navController.navigationBar.prefersLargeTitles = true
        self.navigationController = navController
        
        // Create window
        let window = UIWindow(windowScene: windowScene)
        window.rootViewController = navController
        window.makeKeyAndVisible()
        self.window = window
        
        // Start polling
        startPollTimer()
    }

    func sceneDidDisconnect(_ scene: UIScene) {
        stopPollTimer()
        clientCore?.shutdown()
    }

    func sceneDidBecomeActive(_ scene: UIScene) {
        startPollTimer()
    }

    func sceneWillResignActive(_ scene: UIScene) {
        // Polling continues in background (briefly)
    }

    func sceneWillEnterForeground(_ scene: UIScene) {
        startPollTimer()
    }

    func sceneDidEnterBackground(_ scene: UIScene) {
        // Can stop polling in background to save battery
        // stopPollTimer()
    }
    
    // MARK: - Poll Timer
    
    private func startPollTimer() {
        guard pollTimer == nil else { return }
        pollTimer = Timer.scheduledTimer(withTimeInterval: 0.016, repeats: true) { [weak self] _ in
            self?.pollEvents()
        }
    }
    
    private func stopPollTimer() {
        pollTimer?.invalidate()
        pollTimer = nil
    }
    
    private func pollEvents() {
        guard let clientCore = clientCore else { return }
        
        clientCore.update()
        
        while let event = clientCore.pollEvent() {
            handleEvent(event)
        }
    }
    
    private func handleEvent(_ event: String) {
        guard let data = event.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = json["type"] as? String else {
            print("❌ Failed to parse event: \(event)")
            return
        }
        
        switch type {
        case "connectionStateChanged":
            if let state = json["state"] as? String {
                handleConnectionStateChanged(state: state, json: json)
            }
        case "serverMessage":
            if let messageData = json["data"] {
                handleServerMessage(messageData)
            }
        case "error":
            if let message = json["message"] as? String {
                print("❌ ClientCore error: \(message)")
                showError(message)
                
                // При ошибке соединения возвращаемся к списку серверов
                if sessionListVC != nil && !isConnected {
                    navigationController?.popToRootViewController(animated: true)
                    sessionListVC = nil
                    currentServerAddress = nil
                }
            }
        default:
            print("⚠️ Unknown event: \(type)")
        }
    }
    
    private func handleConnectionStateChanged(state: String, json: [String: Any]) {
        print("🔌 Connection state: \(state)")
        
        switch state {
        case "connecting":
            isConnected = false
            sessionListVC?.showConnecting()
            
        case "connected":
            isConnected = true
            sessionListVC?.showConnected()
            
        case "disconnected":
            handleDisconnected()
            
        default:
            break
        }
    }
    
    private func handleServerMessage(_ data: Any) {
        guard let messageDict = data as? [String: Any],
              let messageType = messageDict["type"] as? String else {
            return
        }
        
        switch messageType {
        case "connected":
            // Server sent initial cwd and home
            if let home = messageDict["home"] as? String {
                print("🏠 Server home: \(home)")
                terminalVC?.serverHome = home
            }
            if let cwd = messageDict["cwd"] as? String {
                print("📂 Initial CWD: \(cwd)")
                terminalVC?.updateCurrentCwd(cwd)
            }
            
        case "sessions_list":
            if let sessionsData = messageDict["sessions"],
               let jsonData = try? JSONSerialization.data(withJSONObject: sessionsData),
               let sessions = try? JSONDecoder().decode([SessionInfo].self, from: jsonData) {
                // Get active session id from client-core
                let activeSessionId = (messageDict["active_session_id"] as? UInt64)
                    ?? (messageDict["active_session_id"] as? Int).map { UInt64($0) }
                print("📋 Sessions: \(sessions.count), active: \(activeSessionId ?? 0)")
                sessionListVC?.updateSessions(sessions, activeId: activeSessionId)
            }
            
        case "output":
            guard isActiveSession(messageDict) else { break }
            // New format: segments array from C++ ANSI parser
            if let segmentsData = messageDict["segments"] {
                do {
                    let jsonData = try JSONSerialization.data(withJSONObject: segmentsData)
                    let segments = try JSONDecoder().decode([StyledSegmentShared].self, from: jsonData)
                    terminalVC?.appendStyledOutput(segments)
                } catch {
                    print("❌ Failed to decode segments: \(error)")
                }
            }
            // Fallback: raw data
            else if let outputData = messageDict["data"] as? String {
                terminalVC?.appendOutput(outputData)
            }
            
        case "history":
            if let commandsData = messageDict["commands"] {
                do {
                    let jsonData = try JSONSerialization.data(withJSONObject: commandsData)
                    let records = try JSONDecoder().decode([CommandHistoryRecordShared].self, from: jsonData)
                    print("📜 History: \(records.count) commands")
                    terminalVC?.loadHistory(records)
                } catch {
                    print("❌ Failed to decode history: \(error)")
                }
            }
            
        case "block_screen_update":
            guard isActiveSession(messageDict) else { break }
            if let updatesData = messageDict["updates"],
               let cursorRow = messageDict["cursor_row"] as? Int,
               let cursorColumn = messageDict["cursor_column"] as? Int {
                do {
                    let jsonData = try JSONSerialization.data(withJSONObject: updatesData)
                    let updates = try JSONDecoder().decode([ScreenRowUpdateShared].self, from: jsonData)
                    terminalVC?.handleBlockScreenUpdate(updates: updates, cursorRow: cursorRow, cursorColumn: cursorColumn)
                } catch {
                    print("❌ Failed to decode block_screen_update: \(error)")
                }
            }
            
        case "command_start":
            guard isActiveSession(messageDict) else { break }
            let cwd = messageDict["cwd"] as? String
            let command = messageDict["command"] as? String
            terminalVC?.didStartCommandBlock(command: command, cwd: cwd)
            
        case "command_end":
            guard isActiveSession(messageDict) else { break }
            let exitCode = messageDict["exit_code"] as? Int ?? 0
            let cwd = messageDict["cwd"] as? String
            terminalVC?.didFinishCommandBlock(exitCode: exitCode, cwd: cwd)
            // Update cwd display if it changed (e.g., after cd)
            if let newCwd = cwd {
                terminalVC?.updateCurrentCwd(newCwd)
            }
            
        case "cwd_update":
            if let cwd = messageDict["cwd"] as? String {
                print("📂 CWD update: \(cwd)")
                terminalVC?.updateCurrentCwd(cwd)
            }
            
        case "error":
            if let message = messageDict["message"] as? String {
                showError(message)
            }
            
        case "prompt_start", "prompt_end":
            break
            
        default:
            // Try AI message handling
            print("🔄 Trying AI message handling for: \(messageType)")
            if !handleAIMessage(messageType, messageDict) {
                print("⚠️ Unhandled message type: \(messageType)")
            }
        }
    }
    
    private func isActiveSession(_ messageDict: [String: Any]) -> Bool {
        guard let sessionId = messageDict["session_id"] as? UInt64
                ?? (messageDict["session_id"] as? Int).map({ UInt64($0) }) else {
            return true
        }
        return sessionId == terminalVC?.sessionId ?? 0
    }
    
    private func handleDisconnected() {
        // Ignore if already disconnected (duplicate events)
        guard sessionListVC != nil || isUserInitiatedDisconnect else { return }
        
        // If user-initiated disconnect — just ignore (pop already happened)
        if isUserInitiatedDisconnect {
            isUserInitiatedDisconnect = false
            isConnected = false
            sessionListVC = nil
            currentServerAddress = nil
            return
        }
        
        // If disconnect happened without successful connection — error
        if !isConnected {
            let address = currentServerAddress ?? "server"
            sessionListVC?.showError("Connection failed")
            showConnectionError("Failed to connect to \(address)")
        }
        
        isConnected = false
        sessionListVC = nil
        currentServerAddress = nil
    }
    
    private func showError(_ message: String) {
        let alert = UIAlertController(title: "Error", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        navigationController?.topViewController?.present(alert, animated: true)
    }
    
    private func showConnectionError(_ message: String) {
        let alert = UIAlertController(title: "Connection Error", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default) { [weak self] _ in
            self?.navigationController?.popToRootViewController(animated: true)
        })
        navigationController?.topViewController?.present(alert, animated: true)
    }
}
