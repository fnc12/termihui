//
//  ViewController.swift
//  termihui-client
//
//  Created by Yevgeniy Zakharov on 05.08.2025.
//

import Cocoa
import SnapKit

/// Корневой контроллер приложения, управляющий навигацией между экранами
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
        determineInitialState()
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
        // Устанавливаем размер окна
        view.frame = NSRect(x: 0, y: 0, width: 800, height: 600)
        preferredContentSize = NSSize(width: 800, height: 600)
    }
    
    private func setupDelegates() {
        welcomeViewController.delegate = self
        connectingViewController.delegate = self
        terminalViewController.delegate = self
        webSocketManager.delegate = self
    }
    
    private func determineInitialState() {
        // Если есть сохраненный адрес, сразу пытаемся подключиться
        if AppSettings.shared.hasServerAddress {
            let serverAddress = AppSettings.shared.serverAddress
            currentState = .connecting(serverAddress: serverAddress)
            // Автоматически инициируем подключение
            webSocketManager.connect(to: serverAddress)
        } else {
            currentState = .welcome
        }
    }
    
    // MARK: - Navigation Methods
    private func updateUIForState(_ state: AppState) {
        // Удаляем текущий дочерний контроллер
        removeCurrentChildController()
        
        // Добавляем новый контроллер в зависимости от состояния
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
        // Удаляем все дочерние контроллеры
        children.forEach { child in
            child.view.removeFromSuperview()
            child.removeFromParent()
        }
    }
    
    private func showWelcomeScreen() {
        addChild(welcomeViewController)
        view.addSubview(welcomeViewController.view)
        
        welcomeViewController.view.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }
    }
    
    private func showConnectingScreen(serverAddress: String) {
        connectingViewController.configure(serverAddress: serverAddress)
        addChild(connectingViewController)
        view.addSubview(connectingViewController.view)
        
        connectingViewController.view.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }
        
        // Подключение инициируется в determineInitialState() или при нажатии кнопки в welcome
        // Здесь только показываем UI
    }
    
    private func showTerminalScreen(serverAddress: String) {
        terminalViewController.configure(serverAddress: serverAddress)
        addChild(terminalViewController)
        view.addSubview(terminalViewController.view)
        
        terminalViewController.view.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }
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
        // Инициируем подключение при ручном вводе адреса
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
        // Отправляем команду через WebSocketManager
        webSocketManager.sendCommand(command)
    }
    
    func terminalViewControllerDidRequestDisconnect(_ controller: TerminalViewController) {
        webSocketManager.disconnect()
        currentState = .welcome
    }
}

// MARK: - WebSocketManagerDelegate
extension ViewController: WebSocketManagerDelegate {
    func webSocketManagerDidConnect(_ manager: WebSocketManager) {
        DispatchQueue.main.async {
            if case .connecting(let serverAddress) = self.currentState {
                self.currentState = .connected(serverAddress: serverAddress)
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
        print("🎯 ViewController получил output: \(output)")
        // Обработка вывода от сервера в терминале
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                print("✅ Найден TerminalViewController, вызываем appendOutput")
                terminalVC.appendOutput(output)
            } else {
                print("❌ TerminalViewController не найден в children")
                print("🔍 Текущие children: \(self.children)")
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveStatus running: Bool, exitCode: Int) {
        // Обработка изменений статуса процесса
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                if !running {
                    terminalVC.appendOutput("Process exited with code \(exitCode)\n")
                }
            }
        }
    }
}

