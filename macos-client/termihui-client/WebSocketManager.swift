import Foundation
import Cocoa

/// Manager for WebSocket connection to TermiHUI server
class WebSocketManager: NSObject {
    
    // MARK: - Properties
    private var webSocketTask: URLSessionWebSocketTask?
    private var urlSession: URLSession?
    weak var delegate: WebSocketManagerDelegate?
    
    private var isConnected = false
    private var serverAddress = ""
    private var lastSentCommand: String? = nil
    
    // MARK: - Public Methods
    
    /// Connect to server
    func connect(to serverAddress: String) {
        print("🔌 Connection attempt to: \(serverAddress)")
        self.serverAddress = serverAddress
        
        // Build URL for WebSocket connection
        guard let url = URL(string: "ws://\(serverAddress)") else {
            print("❌ Invalid URL: ws://\(serverAddress)")
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.invalidURL)
            return
        }
        
        print("🌐 Creating WebSocket connection to: \(url)")
        
        // Create URLSession and WebSocket task
        urlSession = URLSession(configuration: .default, delegate: self, delegateQueue: nil)
        webSocketTask = urlSession?.webSocketTask(with: url)
        
        // Start connection
        webSocketTask?.resume()
        print("▶️ WebSocket task started")
        
        // Start listening for incoming messages
        receiveMessage()
        print("👂 Starting to listen for incoming messages")
    }
    
    /// Disconnect from server
    func disconnect() {
        webSocketTask?.cancel(with: .goingAway, reason: nil)
        webSocketTask = nil
        urlSession?.invalidateAndCancel()
        urlSession = nil
        isConnected = false
    }
    
    /// Send command to server
    func sendCommand(_ command: String) {
        print("📤 Sending command: \(command)")
        guard isConnected else {
            print("❌ Not connected to server")
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.notConnected)
            return
        }
        
        // Save last sent command for block header
        lastSentCommand = command

        let message = TerminalMessage.execute(command: command)
        sendMessage(message)
    }
    
    /// Send input to terminal
    func sendInput(_ input: String) {
        guard isConnected else {
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.notConnected)
            return
        }
        
        let message = TerminalMessage.input(text: input)
        sendMessage(message)
    }
    
    /// Request tab completion
    func requestCompletion(for text: String, cursorPosition: Int) {
        print("📤 Completion request for: '\(text)' (position: \(cursorPosition))")
        guard isConnected else {
            print("❌ Not connected to server for completion")
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.notConnected)
            return
        }
        
        let message = TerminalMessage.completion(text: text, cursorPosition: cursorPosition)
        sendMessage(message)
    }
    
    /// Send terminal resize event
    func sendResize(cols: Int, rows: Int) {
        guard isConnected else {
            return
        }
        
        print("📐 Sending resize: \(cols)x\(rows)")
        let message = TerminalMessage.resize(cols: cols, rows: rows)
        sendMessage(message)
    }
    
    // MARK: - Private Methods
    
    private func sendMessage(_ message: TerminalMessage) {
        do {
            let jsonData = try JSONEncoder().encode(message)
            let jsonString = String(data: jsonData, encoding: .utf8) ?? ""
            
            let webSocketMessage = URLSessionWebSocketTask.Message.string(jsonString)
            webSocketTask?.send(webSocketMessage) { [weak self] error in
                if let error = error {
                    self?.delegate?.webSocketManager(self!, didFailWithError: error)
                }
            }
        } catch {
            delegate?.webSocketManager(self, didFailWithError: error)
        }
    }
    
    private func receiveMessage() {
        webSocketTask?.receive { [weak self] result in
            guard let self = self else { return }
            
            switch result {
            case .success(let message):
                self.handleReceivedMessage(message)
                // Продолжаем слушать следующие сообщения
                self.receiveMessage()
                
            case .failure(let error):
                self.delegate?.webSocketManager(self, didFailWithError: error)
            }
        }
    }
    
    private func handleReceivedMessage(_ message: URLSessionWebSocketTask.Message) {
        switch message {
        case .string(let text):
            handleJSONMessage(text)
        case .data(let data):
            if let text = String(data: data, encoding: .utf8) {
                handleJSONMessage(text)
            }
        @unknown default:
            break
        }
    }
    
    private func handleJSONMessage(_ jsonString: String) {
        print("📨 Received message from server: \(jsonString)")
        guard let jsonData = jsonString.data(using: .utf8) else { 
            print("❌ Failed to convert to JSON data")
            return 
        }
        
        do {
            let response = try JSONDecoder().decode(TerminalResponse.self, from: jsonData)
            print("✅ JSON decoded: type=\(response.type)")
            
            DispatchQueue.main.async {
                switch response.type {
                case "connected":
                    self.delegate?.webSocketManagerDidConnect(self, initialCwd: response.cwd)
                    
                case "output":
                    if let data = response.data {
                        print("🔄 Passing output to delegate: \(data)")
                        self.delegate?.webSocketManager(self, didReceiveOutput: data)
                    } else {
                        print("❌ No data in output message")
                    }
                    
                case "status":
                    if let running = response.running, let exitCode = response.exitCode {
                        self.delegate?.webSocketManager(self, didReceiveStatus: running, exitCode: exitCode)
                    }
                    
                case "error":
                    let errorMessage = response.message ?? "Unknown error"
                    self.delegate?.webSocketManager(self, didFailWithError: WebSocketError.serverError(errorMessage))
                    
                case "input_sent":
                    // Input send confirmation - can be ignored
                    break
                    
                case "completion_result":
                    if let completions = response.completions,
                       let originalText = response.originalText,
                       let cursorPosition = response.cursorPosition {
                        print("🎯 Received completion options: \(completions)")
                        self.delegate?.webSocketManager(self, didReceiveCompletions: completions, 
                                                      originalText: originalText, 
                                                      cursorPosition: cursorPosition)
                    } else {
                        print("❌ Invalid completion_result format")
                    }
                
                case "command_start":
                    print("🎯 Event: command_start, cwd=\(response.cwd ?? "nil")")
                    // Pass last sent command as block header
                    let cmd = self.lastSentCommand
                    self.lastSentCommand = nil
                    self.delegate?.webSocketManager(self, didReceiveCommandStart: cmd, cwd: response.cwd)
                    
                case "command_end":
                    let exitCode = response.exitCode ?? 0
                    print("🏁 Event: command_end (exit=\(exitCode)), cwd=\(response.cwd ?? "nil")")
                    self.delegate?.webSocketManager(self, didReceiveCommandEndWithExitCode: exitCode, cwd: response.cwd)
                    
                case "history":
                    if let commands = response.commands {
                        print("📜 Received history: \(commands.count) commands")
                        self.delegate?.webSocketManager(self, didReceiveHistory: commands)
                    }
                    
                default:
                    print("Unknown message type: \(response.type)")
                }
            }
        } catch {
            delegate?.webSocketManager(self, didFailWithError: error)
        }
    }
}

// MARK: - URLSessionWebSocketDelegate
extension WebSocketManager: URLSessionWebSocketDelegate {
    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didOpenWithProtocol protocolString: String?) {
        print("🎉 WebSocket connection established! Protocol: \(protocolString ?? "none")")
        isConnected = true
        // Connection established, waiting for "connected" message from server
    }
    
    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didCloseWith closeCode: URLSessionWebSocketTask.CloseCode, reason: Data?) {
        print("🔌 WebSocket connection closed. Code: \(closeCode.rawValue)")
        if let reason = reason, let reasonString = String(data: reason, encoding: .utf8) {
            print("📝 Reason: \(reasonString)")
        }
        isConnected = false
        DispatchQueue.main.async {
            self.delegate?.webSocketManagerDidDisconnect(self)
        }
    }
}

// MARK: - URLSessionDelegate (Required for custom delegate)
extension WebSocketManager: URLSessionDelegate {
    func urlSession(_ session: URLSession, didBecomeInvalidWithError error: Error?) {
        isConnected = false
        if let error = error {
            DispatchQueue.main.async {
                self.delegate?.webSocketManager(self, didFailWithError: error)
            }
        }
    }
}



/// WebSocket connection errors
enum WebSocketError: Error, LocalizedError {
    case invalidURL
    case notConnected
    case serverError(String)
    
    var errorDescription: String? {
        switch self {
        case .invalidURL:
            return "Invalid server address"
        case .notConnected:
            return "Not connected to server"
        case .serverError(let message):
            return "Server error: \(message)"
        }
    }
}

// MARK: - Delegate Protocol
protocol WebSocketManagerDelegate: AnyObject {
    func webSocketManagerDidConnect(_ manager: WebSocketManager, initialCwd: String?)
    func webSocketManagerDidDisconnect(_ manager: WebSocketManager)
    func webSocketManager(_ manager: WebSocketManager, didReceiveOutput output: String)
    func webSocketManager(_ manager: WebSocketManager, didReceiveStatus running: Bool, exitCode: Int)
    func webSocketManager(_ manager: WebSocketManager, didFailWithError error: Error)
    func webSocketManager(_ manager: WebSocketManager, didReceiveCompletions completions: [String], originalText: String, cursorPosition: Int)
    // Command events with cwd
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandStart command: String?, cwd: String?)
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandEndWithExitCode exitCode: Int, cwd: String?)
    // Command history
    func webSocketManager(_ manager: WebSocketManager, didReceiveHistory history: [CommandHistoryRecord])
}