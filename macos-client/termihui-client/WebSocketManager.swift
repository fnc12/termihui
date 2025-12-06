import Foundation
import Cocoa

/// Менеджер для управления WebSocket подключением к серверу TermiHUI
class WebSocketManager: NSObject {
    
    // MARK: - Properties
    private var webSocketTask: URLSessionWebSocketTask?
    private var urlSession: URLSession?
    weak var delegate: WebSocketManagerDelegate?
    
    private var isConnected = false
    private var serverAddress = ""
    private var lastSentCommand: String? = nil
    
    // MARK: - Public Methods
    
    /// Подключение к серверу
    func connect(to serverAddress: String) {
        print("🔌 Попытка подключения к: \(serverAddress)")
        self.serverAddress = serverAddress
        
        // Формируем URL для WebSocket подключения
        guard let url = URL(string: "ws://\(serverAddress)") else {
            print("❌ Некорректный URL: ws://\(serverAddress)")
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.invalidURL)
            return
        }
        
        print("🌐 Создание WebSocket подключения к: \(url)")
        
        // Создаем URLSession и WebSocket task
        urlSession = URLSession(configuration: .default, delegate: self, delegateQueue: nil)
        webSocketTask = urlSession?.webSocketTask(with: url)
        
        // Запускаем подключение
        webSocketTask?.resume()
        print("▶️ WebSocket задача запущена")
        
        // Начинаем слушать входящие сообщения
        receiveMessage()
        print("👂 Начинаем слушать входящие сообщения")
    }
    
    /// Отключение от сервера
    func disconnect() {
        webSocketTask?.cancel(with: .goingAway, reason: nil)
        webSocketTask = nil
        urlSession?.invalidateAndCancel()
        urlSession = nil
        isConnected = false
    }
    
    /// Отправка команды на сервер
    func sendCommand(_ command: String) {
        print("📤 Отправка команды: \(command)")
        guard isConnected else {
            print("❌ Не подключен к серверу")
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.notConnected)
            return
        }
        
        // Сохраняем последнюю отправленную команду для заголовка блока
        lastSentCommand = command

        let message = TerminalMessage.execute(command: command)
        sendMessage(message)
    }
    
    /// Отправка ввода в терминал
    func sendInput(_ input: String) {
        guard isConnected else {
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.notConnected)
            return
        }
        
        let message = TerminalMessage.input(text: input)
        sendMessage(message)
    }
    
    /// Запрос автодополнения
    func requestCompletion(for text: String, cursorPosition: Int) {
        print("📤 Запрос автодополнения для: '\(text)' (позиция: \(cursorPosition))")
        guard isConnected else {
            print("❌ Не подключен к серверу для автодополнения")
            delegate?.webSocketManager(self, didFailWithError: WebSocketError.notConnected)
            return
        }
        
        let message = TerminalMessage.completion(text: text, cursorPosition: cursorPosition)
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
        print("📨 Получено сообщение от сервера: \(jsonString)")
        guard let jsonData = jsonString.data(using: .utf8) else { 
            print("❌ Не удалось преобразовать в JSON data")
            return 
        }
        
        do {
            let response = try JSONDecoder().decode(TerminalResponse.self, from: jsonData)
            print("✅ JSON декодирован: type=\(response.type)")
            
            DispatchQueue.main.async {
                switch response.type {
                case "connected":
                    self.delegate?.webSocketManagerDidConnect(self, initialCwd: response.cwd)
                    
                case "output":
                    if let data = response.data {
                        print("🔄 Передаем output в delegate: \(data)")
                        self.delegate?.webSocketManager(self, didReceiveOutput: data)
                    } else {
                        print("❌ Нет data в output сообщении")
                    }
                    
                case "status":
                    if let running = response.running, let exitCode = response.exitCode {
                        self.delegate?.webSocketManager(self, didReceiveStatus: running, exitCode: exitCode)
                    }
                    
                case "error":
                    let errorMessage = response.message ?? "Unknown error"
                    self.delegate?.webSocketManager(self, didFailWithError: WebSocketError.serverError(errorMessage))
                    
                case "input_sent":
                    // Подтверждение отправки ввода - можно игнорировать
                    break
                    
                case "completion_result":
                    if let completions = response.completions,
                       let originalText = response.originalText,
                       let cursorPosition = response.cursorPosition {
                        print("🎯 Получены варианты автодополнения: \(completions)")
                        self.delegate?.webSocketManager(self, didReceiveCompletions: completions, 
                                                      originalText: originalText, 
                                                      cursorPosition: cursorPosition)
                    } else {
                        print("❌ Некорректный формат completion_result")
                    }
                
                case "command_start":
                    print("🎯 Событие: command_start, cwd=\(response.cwd ?? "nil")")
                    // Передаем последнюю отправленную команду как заголовок блока
                    let cmd = self.lastSentCommand
                    self.lastSentCommand = nil
                    self.delegate?.webSocketManager(self, didReceiveCommandStart: cmd, cwd: response.cwd)
                    
                case "command_end":
                    let exitCode = response.exitCode ?? 0
                    print("🏁 Событие: command_end (exit=\(exitCode)), cwd=\(response.cwd ?? "nil")")
                    self.delegate?.webSocketManager(self, didReceiveCommandEndWithExitCode: exitCode, cwd: response.cwd)
                    
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
        print("🎉 WebSocket подключение установлено! Протокол: \(protocolString ?? "none")")
        isConnected = true
        // Подключение установлено, ждем сообщение "connected" от сервера
    }
    
    func urlSession(_ session: URLSession, webSocketTask: URLSessionWebSocketTask, didCloseWith closeCode: URLSessionWebSocketTask.CloseCode, reason: Data?) {
        print("🔌 WebSocket подключение закрыто. Код: \(closeCode.rawValue)")
        if let reason = reason, let reasonString = String(data: reason, encoding: .utf8) {
            print("📝 Причина: \(reasonString)")
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



/// Ошибки WebSocket подключения
enum WebSocketError: Error, LocalizedError {
    case invalidURL
    case notConnected
    case serverError(String)
    
    var errorDescription: String? {
        switch self {
        case .invalidURL:
            return "Неверный адрес сервера"
        case .notConnected:
            return "Нет подключения к серверу"
        case .serverError(let message):
            return "Ошибка сервера: \(message)"
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
    // Командные события с cwd
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandStart command: String?, cwd: String?)
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandEndWithExitCode exitCode: Int, cwd: String?)
}