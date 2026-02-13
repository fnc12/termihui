import Cocoa
import SnapKit

/// Sheet view controller for adding or editing an LLM provider
final class LLMProviderEditViewController: NSViewController {
    
    // MARK: - Properties
    private let existingProvider: LLMProvider?
    var onSave: ((_ name: String, _ url: String, _ model: String, _ apiKey: String) -> Void)?
    
    private var isEditMode: Bool { existingProvider != nil }
    
    // MARK: - UI Components
    private let titleLabel = NSTextField(labelWithString: "")
    private let nameLabel = NSTextField(labelWithString: "Name:")
    private let nameField = NSTextField()
    private let urlLabel = NSTextField(labelWithString: "URL:")
    private let urlField = NSTextField()
    private let modelLabel = NSTextField(labelWithString: "Model:")
    private let modelField = NSTextField()
    private let apiKeyLabel = NSTextField(labelWithString: "API Key:")
    private let apiKeyField = NSSecureTextField()
    private let cancelButton = NSButton()
    private let saveButton = NSButton()
    
    // MARK: - Init
    
    init(provider: LLMProvider?) {
        self.existingProvider = provider
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Lifecycle
    
    override func loadView() {
        view = NSView(frame: NSRect(x: 0, y: 0, width: 400, height: 260))
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        populateFields()
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        let labelWidth: CGFloat = 70
        let fieldHeight: CGFloat = 22
        let rowSpacing: CGFloat = 12
        
        // Title
        titleLabel.stringValue = isEditMode ? "Edit Provider" : "Add Provider"
        titleLabel.font = NSFont.systemFont(ofSize: 15, weight: .semibold)
        titleLabel.textColor = .labelColor
        view.addSubview(titleLabel)
        
        titleLabel.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(16)
            make.leading.equalToSuperview().offset(16)
        }
        
        // Name row
        nameLabel.alignment = .right
        nameLabel.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(nameLabel)
        
        nameField.placeholderString = "My Provider"
        nameField.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(nameField)
        
        nameLabel.snp.makeConstraints { make in
            make.top.equalTo(titleLabel.snp.bottom).offset(16)
            make.leading.equalToSuperview().offset(16)
            make.width.equalTo(labelWidth)
        }
        
        nameField.snp.makeConstraints { make in
            make.centerY.equalTo(nameLabel)
            make.leading.equalTo(nameLabel.snp.trailing).offset(8)
            make.trailing.equalToSuperview().offset(-16)
            make.height.equalTo(fieldHeight)
        }
        
        // URL row
        urlLabel.alignment = .right
        urlLabel.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(urlLabel)
        
        urlField.placeholderString = "http://localhost:11434"
        urlField.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(urlField)
        
        urlLabel.snp.makeConstraints { make in
            make.top.equalTo(nameLabel.snp.bottom).offset(rowSpacing)
            make.leading.equalToSuperview().offset(16)
            make.width.equalTo(labelWidth)
        }
        
        urlField.snp.makeConstraints { make in
            make.centerY.equalTo(urlLabel)
            make.leading.equalTo(urlLabel.snp.trailing).offset(8)
            make.trailing.equalToSuperview().offset(-16)
            make.height.equalTo(fieldHeight)
        }
        
        // Model row
        modelLabel.alignment = .right
        modelLabel.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(modelLabel)
        
        modelField.placeholderString = "llama3 (optional)"
        modelField.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(modelField)
        
        modelLabel.snp.makeConstraints { make in
            make.top.equalTo(urlLabel.snp.bottom).offset(rowSpacing)
            make.leading.equalToSuperview().offset(16)
            make.width.equalTo(labelWidth)
        }
        
        modelField.snp.makeConstraints { make in
            make.centerY.equalTo(modelLabel)
            make.leading.equalTo(modelLabel.snp.trailing).offset(8)
            make.trailing.equalToSuperview().offset(-16)
            make.height.equalTo(fieldHeight)
        }
        
        // API Key row
        apiKeyLabel.alignment = .right
        apiKeyLabel.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(apiKeyLabel)
        
        apiKeyField.placeholderString = isEditMode ? "Leave empty to keep current" : "sk-... (optional)"
        apiKeyField.font = NSFont.systemFont(ofSize: 13)
        view.addSubview(apiKeyField)
        
        apiKeyLabel.snp.makeConstraints { make in
            make.top.equalTo(modelLabel.snp.bottom).offset(rowSpacing)
            make.leading.equalToSuperview().offset(16)
            make.width.equalTo(labelWidth)
        }
        
        apiKeyField.snp.makeConstraints { make in
            make.centerY.equalTo(apiKeyLabel)
            make.leading.equalTo(apiKeyLabel.snp.trailing).offset(8)
            make.trailing.equalToSuperview().offset(-16)
            make.height.equalTo(fieldHeight)
        }
        
        // Buttons
        cancelButton.title = "Cancel"
        cancelButton.bezelStyle = .rounded
        cancelButton.keyEquivalent = "\u{1b}" // Escape
        cancelButton.target = self
        cancelButton.action = #selector(cancelClicked)
        view.addSubview(cancelButton)
        
        saveButton.title = isEditMode ? "Save" : "Add"
        saveButton.bezelStyle = .rounded
        saveButton.keyEquivalent = "\r"
        saveButton.target = self
        saveButton.action = #selector(saveClicked)
        view.addSubview(saveButton)
        
        saveButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-16)
            make.bottom.equalToSuperview().offset(-16)
        }
        
        cancelButton.snp.makeConstraints { make in
            make.trailing.equalTo(saveButton.snp.leading).offset(-8)
            make.centerY.equalTo(saveButton)
        }
    }
    
    private func populateFields() {
        guard let provider = existingProvider else { return }
        nameField.stringValue = provider.name
        urlField.stringValue = provider.url
        modelField.stringValue = provider.model
        // API key is never shown — server doesn't send it
    }
    
    // MARK: - Actions
    
    @objc private func cancelClicked() {
        dismiss(nil)
    }
    
    @objc private func saveClicked() {
        let name = nameField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        let url = urlField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        let model = modelField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        let apiKey = apiKeyField.stringValue
        
        guard !name.isEmpty else {
            showValidationError("Name is required")
            return
        }
        
        guard !url.isEmpty else {
            showValidationError("URL is required")
            return
        }
        
        onSave?(name, url, model, apiKey)
        dismiss(nil)
    }
    
    private func showValidationError(_ message: String) {
        let alert = NSAlert()
        alert.messageText = "Validation Error"
        alert.informativeText = message
        alert.alertStyle = .warning
        alert.addButton(withTitle: "OK")
        alert.beginSheetModal(for: view.window!)
    }
}
