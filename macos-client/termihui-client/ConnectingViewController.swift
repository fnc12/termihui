import Cocoa
import SnapKit

/// Экран подключения с индикатором прогресса
class ConnectingViewController: NSViewController {
    
    // MARK: - UI Components
    private let titleLabel = NSTextField(labelWithString: "Подключение...")
    private let progressIndicator = NSProgressIndicator()
    private let statusLabel = NSTextField(labelWithString: "Подключение к серверу")
    private let cancelButton = NSButton(title: "Отмена", target: nil, action: nil)
    
    // MARK: - Properties
    weak var delegate: ConnectingViewControllerDelegate?
    private var serverAddress: String = ""
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupLayout()
        setupActions()
    }
    
    override func viewDidAppear() {
        super.viewDidAppear()
        progressIndicator.startAnimation(nil)
    }
    
    override func viewDidDisappear() {
        super.viewDidDisappear()
        progressIndicator.stopAnimation(nil)
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
        // Title label
        titleLabel.font = NSFont.systemFont(ofSize: 18, weight: .medium)
        titleLabel.alignment = .center
        
        // Progress indicator
        progressIndicator.style = .spinning
        progressIndicator.controlSize = .regular
        
        // Status label
        statusLabel.font = NSFont.systemFont(ofSize: 13)
        statusLabel.alignment = .center
        statusLabel.textColor = .secondaryLabelColor
        
        // Cancel button
        cancelButton.bezelStyle = .rounded
        cancelButton.target = self
        cancelButton.action = #selector(cancelButtonTapped)
        cancelButton.keyEquivalent = "\u{1b}" // Escape key
        
        // Add to view
        view.addSubview(titleLabel)
        view.addSubview(progressIndicator)
        view.addSubview(statusLabel)
        view.addSubview(cancelButton)
    }
    
    private func setupLayout() {
        titleLabel.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.centerY.equalToSuperview().offset(-50)
        }
        
        progressIndicator.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.top.equalTo(titleLabel.snp.bottom).offset(20)
            make.width.height.equalTo(32)
        }
        
        statusLabel.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.top.equalTo(progressIndicator.snp.bottom).offset(20)
        }
        
        cancelButton.snp.makeConstraints { make in
            make.centerX.equalToSuperview()
            make.top.equalTo(statusLabel.snp.bottom).offset(30)
            make.width.equalTo(100)
        }
    }
    
    private func setupActions() {
        // Actions already set in setupUI
    }
    
    // MARK: - Public Methods
    func configure(serverAddress: String) {
        self.serverAddress = serverAddress
        statusLabel.stringValue = "Подключение к \(serverAddress)"
    }
    
    func updateStatus(_ status: String) {
        statusLabel.stringValue = status
    }
    
    // MARK: - Actions
    @objc private func cancelButtonTapped() {
        delegate?.connectingViewControllerDidCancel(self)
    }
}

// MARK: - Delegate Protocol
protocol ConnectingViewControllerDelegate: AnyObject {
    func connectingViewControllerDidCancel(_ controller: ConnectingViewController)
}