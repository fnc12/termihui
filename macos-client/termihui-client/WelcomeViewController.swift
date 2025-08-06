import Cocoa
import SnapKit

/// Экран приветствия для ввода адреса сервера
class WelcomeViewController: NSViewController {
    
    // MARK: - UI Components
    private let titleLabel = NSTextField(labelWithString: "TermiHUI")
    private let subtitleLabel = NSTextField(labelWithString: "Подключение к терминальному серверу")
    private let serverAddressTextField = NSTextField()
    private let connectButton = NSButton(title: "Подключиться", target: nil, action: nil)
    
    // MARK: - Properties
    weak var delegate: WelcomeViewControllerDelegate?
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupLayout()
        setupActions()
        loadDefaultServerAddress()
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
        // Title label
        titleLabel.font = NSFont.systemFont(ofSize: 24, weight: .bold)
        titleLabel.alignment = .center
        
        // Subtitle label
        subtitleLabel.font = NSFont.systemFont(ofSize: 14)
        subtitleLabel.alignment = .center
        subtitleLabel.textColor = .secondaryLabelColor
        
        // Server address text field
        serverAddressTextField.placeholderString = "localhost:37854"
        serverAddressTextField.font = NSFont.monospacedSystemFont(ofSize: 13, weight: .regular)
        
        // Connect button
        connectButton.bezelStyle = .rounded
        connectButton.target = self
        connectButton.action = #selector(connectButtonTapped)
        connectButton.keyEquivalent = "\r" // Enter key
        
        // Add to view
        view.addSubview(titleLabel)
        view.addSubview(subtitleLabel)
        view.addSubview(serverAddressTextField)
        view.addSubview(connectButton)
    }
    
    private func setupLayout() {
        titleLabel.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.top.equalToSuperview().offset(80)
        }
        
        subtitleLabel.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.top.equalTo(titleLabel.snp.bottom).offset(8)
        }
        
        serverAddressTextField.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.top.equalTo(subtitleLabel.snp.bottom).offset(40)
            make.width.equalTo(300)
            make.height.equalTo(24)
        }
        
        connectButton.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.top.equalTo(serverAddressTextField.snp.bottom).offset(20)
            make.width.equalTo(120)
        }
    }
    
    private func setupActions() {
        serverAddressTextField.target = self
        serverAddressTextField.action = #selector(connectButtonTapped)
    }
    
    private func loadDefaultServerAddress() {
        serverAddressTextField.stringValue = AppSettings.shared.serverAddress
    }
    
    // MARK: - Actions
    @objc private func connectButtonTapped() {
        let serverAddress = serverAddressTextField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        
        guard !serverAddress.isEmpty else {
            showErrorAlert(message: "Пожалуйста, введите адрес сервера")
            return
        }
        
        // Сохраняем адрес для следующего раза
        AppSettings.shared.serverAddress = serverAddress
        
        // Уведомляем delegate о попытке подключения
        delegate?.welcomeViewController(self, didRequestConnectionTo: serverAddress)
    }
    
    private func showErrorAlert(message: String) {
        let alert = NSAlert()
        alert.messageText = "Ошибка"
        alert.informativeText = message
        alert.alertStyle = .warning
        alert.addButton(withTitle: "OK")
        
        if let window = view.window {
            alert.beginSheetModal(for: window)
        } else {
            alert.runModal()
        }
    }
}

// MARK: - Delegate Protocol
protocol WelcomeViewControllerDelegate: AnyObject {
    func welcomeViewController(_ controller: WelcomeViewController, didRequestConnectionTo serverAddress: String)
}