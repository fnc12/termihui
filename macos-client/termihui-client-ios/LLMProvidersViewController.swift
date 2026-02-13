import UIKit

/// Table view controller showing a list of LLM providers with add/edit/delete
final class LLMProvidersViewController: UITableViewController {
    
    // MARK: - Properties
    weak var delegate: LLMProvidersViewControllerDelegate?
    private var providers: [LLMProvider] = []
    
    // MARK: - Init
    
    init(providers: [LLMProvider]) {
        self.providers = providers
        super.init(style: .insetGrouped)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Lifecycle
    
    override func viewDidLoad() {
        super.viewDidLoad()
        title = "LLM Providers"
        navigationItem.largeTitleDisplayMode = .never
        
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add,
            target: self,
            action: #selector(addTapped)
        )
        
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "ProviderCell")
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        if isMovingFromParent {
            delegate?.llmProvidersDidDismiss()
        }
    }
    
    // MARK: - Public
    
    func updateProviders(_ newProviders: [LLMProvider]) {
        providers = newProviders
        tableView.reloadData()
    }
    
    // MARK: - Actions
    
    @objc private func addTapped() {
        let editVC = LLMProviderEditViewController(provider: nil)
        editVC.onSave = { [weak self] name, url, model, apiKey in
            self?.delegate?.llmProvidersDidRequestAdd(name: name, url: url, model: model, apiKey: apiKey)
        }
        navigationController?.pushViewController(editVC, animated: true)
    }
    
    // MARK: - UITableViewDataSource
    
    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return providers.count
    }
    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "ProviderCell", for: indexPath)
        let provider = providers[indexPath.row]
        
        var config = cell.defaultContentConfiguration()
        config.text = provider.name
        
        var details = provider.url
        if !provider.model.isEmpty {
            details += " • \(provider.model)"
        }
        config.secondaryText = details
        config.secondaryTextProperties.color = .secondaryLabel
        config.secondaryTextProperties.font = .systemFont(ofSize: 13)
        
        cell.contentConfiguration = config
        cell.accessoryType = .disclosureIndicator
        
        return cell
    }
    
    // MARK: - UITableViewDelegate
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        let provider = providers[indexPath.row]
        
        let editVC = LLMProviderEditViewController(provider: provider)
        editVC.onSave = { [weak self] name, url, model, apiKey in
            self?.delegate?.llmProvidersDidRequestUpdate(
                id: provider.id, name: name, url: url, model: model, apiKey: apiKey
            )
        }
        navigationController?.pushViewController(editVC, animated: true)
    }
    
    override func tableView(_ tableView: UITableView, commit editingStyle: UITableViewCell.EditingStyle, forRowAt indexPath: IndexPath) {
        if editingStyle == .delete {
            let provider = providers[indexPath.row]
            delegate?.llmProvidersDidRequestDelete(id: provider.id)
        }
    }
    
    override func tableView(_ tableView: UITableView, titleForDeleteConfirmationButtonForRowAt indexPath: IndexPath) -> String? {
        return "Delete"
    }
}
