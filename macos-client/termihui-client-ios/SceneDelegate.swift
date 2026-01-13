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
        case "sessions_list":
            if let sessionsData = messageDict["sessions"],
               let jsonData = try? JSONSerialization.data(withJSONObject: sessionsData),
               let sessions = try? JSONDecoder().decode([SessionInfo].self, from: jsonData) {
                print("📋 Sessions: \(sessions.count)")
                sessionListVC?.updateSessions(sessions)
            }
            
        case "error":
            if let message = messageDict["message"] as? String {
                showError(message)
            }
            
        default:
            // Ignore other messages for MVP
            break
        }
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
            showError("Failed to connect to \(address)")
            navigationController?.popToRootViewController(animated: true)
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
}
