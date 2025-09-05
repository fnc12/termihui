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
        
        // Не устанавливаем фиксированный размер - позволяем окну быть изменяемым
        // view.frame и preferredContentSize убираем для гибкости
    }
    
    private func setupWindow() {
        guard let window = view.window else { return }
        
        // Делаем окно изменяемым по размеру
        window.styleMask.insert(.resizable)
        
        // Добавляем поддержку полноэкранного режима
        window.collectionBehavior = [.fullScreenPrimary]
        
        // Устанавливаем начальный размер и минимальные размеры
        window.setContentSize(NSSize(width: 800, height: 600))
        window.minSize = NSSize(width: 400, height: 300)
        
        // Центрируем окно на экране
        window.center()
        
        // Управляем размером ViewController.view вручную через frame
        if let contentView = window.contentView {
            view.translatesAutoresizingMaskIntoConstraints = true // Включаем autoresizing
            view.frame = contentView.bounds
            print("🔧 Установили начальный frame ViewController: \(view.frame)")
        }
        
        print("🔧 Окно настроено: изменяемый размер, минимум 400x300, поддержка полного экрана")
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
        
        print("🔧 Окно изменило размер: \(window.frame.size)")
        
        // ПРИНУДИТЕЛЬНО устанавливаем frame ViewController под размер окна
        if let contentView = window.contentView {
            view.frame = contentView.bounds
            print("🔧 Установили frame ViewController: \(view.frame)")
        }
        
        // Обновляем frame дочерних view controllers
        updateChildViewFrame()
        
        // Принудительно обновляем layout всех дочерних контроллеров
        DispatchQueue.main.async {
            self.view.layoutSubtreeIfNeeded()
            self.children.forEach { child in
                child.view.layoutSubtreeIfNeeded()
            }
        }
    }
    
    private func updateChildViewFrame() {
        // Устанавливаем frame всех дочерних view controllers = размеру parent view
        children.forEach { child in
            child.view.frame = view.bounds
            print("🔧 Обновили frame дочернего контроллера: \(child.view.frame)")
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
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
        updateChildViewFrame()
    }
    
    private func showConnectingScreen(serverAddress: String) {
        connectingViewController.configure(serverAddress: serverAddress)
        addChild(connectingViewController)
        view.addSubview(connectingViewController.view)
        updateChildViewFrame()
        
        // Подключение инициируется в determineInitialState() или при нажатии кнопки в welcome
        // Здесь только показываем UI
    }
    
    private func showTerminalScreen(serverAddress: String) {
        print("🔧 showTerminalScreen: Parent view размер: \(view.frame)")
        print("🔧 showTerminalScreen: Window размер: \(view.window?.frame.size ?? CGSize.zero)")
        
        terminalViewController.configure(serverAddress: serverAddress)
        terminalViewController.webSocketManager = webSocketManager
        addChild(terminalViewController)
        view.addSubview(terminalViewController.view)
        updateChildViewFrame()
        
        print("🔧 showTerminalScreen: Terminal view добавлен с frame")
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
                if !running && exitCode != 0 {
                    // Показываем сообщение об ошибке только при неудачном завершении
                    terminalVC.appendOutput("❌ Process exited with code \(exitCode)\n")
                }
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveCompletions completions: [String], originalText: String, cursorPosition: Int) {
        print("🎯 ViewController получил варианты автодополнения:")
        print("   Исходный текст: '\(originalText)'")
        print("   Позиция курсора: \(cursorPosition)")
        print("   Варианты: \(completions)")
        
        // Передаем варианты автодополнения в TerminalViewController для обработки
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                terminalVC.handleCompletionResults(completions, originalText: originalText, cursorPosition: cursorPosition)
            }
        }
    }

    // MARK: - Command events
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandStart command: String?) {
        // Передаем команду как заголовок блока (если есть)
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                if let cmd = command, !cmd.isEmpty {
                    terminalVC.didStartCommandBlock(command: cmd)
                } else {
                    terminalVC.didStartCommandBlock()
                }
            }
        }
    }
    
    func webSocketManager(_ manager: WebSocketManager, didReceiveCommandEndWithExitCode exitCode: Int) {
        // Заглушка: уведомляем TerminalViewController о завершении блока
        DispatchQueue.main.async {
            if let terminalVC = self.children.first(where: { $0 is TerminalViewController }) as? TerminalViewController {
                terminalVC.didFinishCommandBlock(exitCode: exitCode)
            }
        }
    }
}

