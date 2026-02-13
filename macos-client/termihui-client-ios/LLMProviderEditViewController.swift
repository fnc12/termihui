import UIKit

/// View controller for adding or editing an LLM provider on iOS
final class LLMProviderEditViewController: UITableViewController {
    
    // MARK: - Properties
    private let existingProvider: LLMProvider?
    var onSave: ((_ name: String, _ url: String, _ model: String, _ apiKey: String) -> Void)?
    
    private var isEditMode: Bool { existingProvider != nil }
    
    // MARK: - Fields
    private let nameField = UITextField()
    private let urlField = UITextField()
    private let modelField = UITextField()
    private let apiKeyField = UITextField()
    
    private enum Row: Int, CaseIterable {
        case name = 0
        case url
        case model
        case apiKey
    }
    
    // MARK: - Init
    
    init(provider: LLMProvider?) {
        self.existingProvider = provider
        super.init(style: .insetGrouped)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Lifecycle
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = isEditMode ? "Edit Provider" : "Add Provider"
        navigationItem.largeTitleDisplayMode = .never
        
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: isEditMode ? "Save" : "Add",
            style: .done,
            target: self,
            action: #selector(saveTapped)
        )
        
        setupFields()
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "FieldCell")
    }
    
    // MARK: - Setup
    
    private func setupFields() {
        configureTextField(nameField, placeholder: "My Provider")
        configureTextField(urlField, placeholder: "http://localhost:11434")
        urlField.keyboardType = .URL
        urlField.autocapitalizationType = .none
        urlField.autocorrectionType = .no
        
        configureTextField(modelField, placeholder: "llama3 (optional)")
        modelField.autocapitalizationType = .none
        modelField.autocorrectionType = .no
        
        configureTextField(apiKeyField, placeholder: isEditMode ? "Leave empty to keep current" : "sk-... (optional)")
        apiKeyField.isSecureTextEntry = true
        apiKeyField.autocapitalizationType = .none
        apiKeyField.autocorrectionType = .no
        
        // Populate from existing provider
        if let provider = existingProvider {
            nameField.text = provider.name
            urlField.text = provider.url
            modelField.text = provider.model
        }
    }
    
    private func configureTextField(_ textField: UITextField, placeholder: String) {
        textField.placeholder = placeholder
        textField.font = .systemFont(ofSize: 16)
        textField.clearButtonMode = .whileEditing
        textField.returnKeyType = .next
    }
    
    // MARK: - Actions
    
    @objc private func saveTapped() {
        let name = (nameField.text ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        let url = (urlField.text ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        let model = (modelField.text ?? "").trimmingCharacters(in: .whitespacesAndNewlines)
        let apiKey = apiKeyField.text ?? ""
        
        guard !name.isEmpty else {
            showValidationError("Name is required")
            return
        }
        
        guard !url.isEmpty else {
            showValidationError("URL is required")
            return
        }
        
        onSave?(name, url, model, apiKey)
        navigationController?.popViewController(animated: true)
    }
    
    private func showValidationError(_ message: String) {
        let alert = UIAlertController(title: "Validation Error", message: message, preferredStyle: .alert)
        alert.addAction(UIAlertAction(title: "OK", style: .default))
        present(alert, animated: true)
    }
    
    // MARK: - UITableViewDataSource
    
    override func numberOfSections(in tableView: UITableView) -> Int {
        return 1
    }
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return Row.allCases.count
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "FieldCell", for: indexPath)
        cell.selectionStyle = .none
        
        // Remove old subviews
        cell.contentView.subviews.forEach { $0.removeFromSuperview() }
        
        guard let row = Row(rawValue: indexPath.row) else { return cell }
        
        let label = UILabel()
        label.font = .systemFont(ofSize: 14, weight: .medium)
        label.textColor = .secondaryLabel
        label.translatesAutoresizingMaskIntoConstraints = false
        cell.contentView.addSubview(label)
        
        let textField: UITextField
        
        switch row {
        case .name:
            label.text = "Name"
            textField = nameField
        case .url:
            label.text = "URL"
            textField = urlField
        case .model:
            label.text = "Model"
            textField = modelField
        case .apiKey:
            label.text = "API Key"
            textField = apiKeyField
        }
        
        textField.translatesAutoresizingMaskIntoConstraints = false
        cell.contentView.addSubview(textField)
        
        NSLayoutConstraint.activate([
            label.topAnchor.constraint(equalTo: cell.contentView.topAnchor, constant: 8),
            label.leadingAnchor.constraint(equalTo: cell.contentView.leadingAnchor, constant: 16),
            label.trailingAnchor.constraint(equalTo: cell.contentView.trailingAnchor, constant: -16),
            
            textField.topAnchor.constraint(equalTo: label.bottomAnchor, constant: 4),
            textField.leadingAnchor.constraint(equalTo: cell.contentView.leadingAnchor, constant: 16),
            textField.trailingAnchor.constraint(equalTo: cell.contentView.trailingAnchor, constant: -16),
            textField.bottomAnchor.constraint(equalTo: cell.contentView.bottomAnchor, constant: -8),
            textField.heightAnchor.constraint(greaterThanOrEqualToConstant: 30),
        ])
        
        return cell
    }
}
