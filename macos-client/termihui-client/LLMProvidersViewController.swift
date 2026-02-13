import Cocoa
import SnapKit

/// Sheet view controller showing a list of LLM providers with add/edit/delete actions
final class LLMProvidersViewController: NSViewController {
    
    // MARK: - Properties
    weak var delegate: LLMProvidersViewControllerDelegate?
    var providers: [LLMProvider] = []
    
    // MARK: - UI Components
    private let titleLabel = NSTextField(labelWithString: "LLM Providers")
    private let scrollView = NSScrollView()
    private let tableView = NSTableView()
    private let addButton = NSButton()
    private let removeButton = NSButton()
    private let editButton = NSButton()
    private let doneButton = NSButton()
    
    // MARK: - Init
    
    init(providers: [LLMProvider]) {
        self.providers = providers
        super.init(nibName: nil, bundle: nil)
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Lifecycle
    
    override func loadView() {
        view = NSView(frame: NSRect(x: 0, y: 0, width: 480, height: 400))
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }
    
    // MARK: - Public
    
    func updateProviders(_ newProviders: [LLMProvider]) {
        providers = newProviders
        tableView.reloadData()
        updateButtonStates()
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        // Title
        titleLabel.font = NSFont.systemFont(ofSize: 16, weight: .semibold)
        titleLabel.textColor = .labelColor
        view.addSubview(titleLabel)
        
        titleLabel.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(16)
            make.leading.equalToSuperview().offset(16)
        }
        
        // Done button (top right)
        doneButton.title = "Done"
        doneButton.bezelStyle = .rounded
        doneButton.keyEquivalent = "\r"
        doneButton.target = self
        doneButton.action = #selector(doneClicked)
        view.addSubview(doneButton)
        
        doneButton.snp.makeConstraints { make in
            make.centerY.equalTo(titleLabel)
            make.trailing.equalToSuperview().offset(-16)
        }
        
        // Bottom button bar
        let buttonBar = NSView()
        view.addSubview(buttonBar)
        
        buttonBar.snp.makeConstraints { make in
            make.leading.trailing.bottom.equalToSuperview().inset(16)
            make.height.equalTo(28)
        }
        
        // Add button (+)
        addButton.bezelStyle = .roundRect
        addButton.image = NSImage(systemSymbolName: "plus", accessibilityDescription: "Add")
        addButton.imagePosition = .imageOnly
        addButton.target = self
        addButton.action = #selector(addClicked)
        buttonBar.addSubview(addButton)
        
        addButton.snp.makeConstraints { make in
            make.leading.centerY.equalToSuperview()
            make.width.equalTo(32)
        }
        
        // Remove button (-)
        removeButton.bezelStyle = .roundRect
        removeButton.image = NSImage(systemSymbolName: "minus", accessibilityDescription: "Remove")
        removeButton.imagePosition = .imageOnly
        removeButton.target = self
        removeButton.action = #selector(removeClicked)
        buttonBar.addSubview(removeButton)
        
        removeButton.snp.makeConstraints { make in
            make.leading.equalTo(addButton.snp.trailing).offset(4)
            make.centerY.equalToSuperview()
            make.width.equalTo(32)
        }
        
        // Edit button
        editButton.bezelStyle = .roundRect
        editButton.title = "Edit"
        editButton.target = self
        editButton.action = #selector(editClicked)
        buttonBar.addSubview(editButton)
        
        editButton.snp.makeConstraints { make in
            make.leading.equalTo(removeButton.snp.trailing).offset(8)
            make.centerY.equalToSuperview()
        }
        
        // Table view
        setupTableView()
        view.addSubview(scrollView)
        
        scrollView.snp.makeConstraints { make in
            make.top.equalTo(titleLabel.snp.bottom).offset(12)
            make.leading.trailing.equalToSuperview().inset(16)
            make.bottom.equalTo(buttonBar.snp.top).offset(-12)
        }
        
        updateButtonStates()
    }
    
    private func setupTableView() {
        let nameColumn = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("NameColumn"))
        nameColumn.title = "Name"
        nameColumn.width = 140
        nameColumn.minWidth = 80
        tableView.addTableColumn(nameColumn)
        
        let urlColumn = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("URLColumn"))
        urlColumn.title = "URL"
        urlColumn.width = 200
        urlColumn.minWidth = 100
        tableView.addTableColumn(urlColumn)
        
        let modelColumn = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("ModelColumn"))
        modelColumn.title = "Model"
        modelColumn.width = 100
        modelColumn.minWidth = 60
        tableView.addTableColumn(modelColumn)
        
        tableView.delegate = self
        tableView.dataSource = self
        tableView.doubleAction = #selector(tableDoubleClicked)
        tableView.target = self
        tableView.usesAlternatingRowBackgroundColors = true
        tableView.columnAutoresizingStyle = .lastColumnOnlyAutoresizingStyle
        
        scrollView.documentView = tableView
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.borderType = .bezelBorder
    }
    
    func updateButtonStates() {
        let hasSelection = tableView.selectedRow >= 0
        removeButton.isEnabled = hasSelection
        editButton.isEnabled = hasSelection
    }
    
    // MARK: - Actions
    
    @objc private func addClicked() {
        let editVC = LLMProviderEditViewController(provider: nil)
        editVC.onSave = { [weak self] name, url, model, apiKey in
            self?.delegate?.llmProvidersDidRequestAdd(name: name, url: url, model: model, apiKey: apiKey)
        }
        presentAsSheet(editVC)
    }
    
    @objc private func removeClicked() {
        let row = tableView.selectedRow
        guard row >= 0 && row < providers.count else { return }
        let provider = providers[row]
        
        let alert = NSAlert()
        alert.messageText = "Delete Provider"
        alert.informativeText = "Are you sure you want to delete \"\(provider.name)\"?"
        alert.alertStyle = .warning
        alert.addButton(withTitle: "Delete")
        alert.addButton(withTitle: "Cancel")
        
        alert.beginSheetModal(for: view.window!) { response in
            if response == .alertFirstButtonReturn {
                self.delegate?.llmProvidersDidRequestDelete(id: provider.id)
            }
        }
    }
    
    @objc private func editClicked() {
        let row = tableView.selectedRow
        guard row >= 0 && row < providers.count else { return }
        showEditSheet(for: providers[row])
    }
    
    @objc private func tableDoubleClicked() {
        let row = tableView.clickedRow
        guard row >= 0 && row < providers.count else { return }
        showEditSheet(for: providers[row])
    }
    
    @objc private func doneClicked() {
        delegate?.llmProvidersDidDismiss()
        dismiss(nil)
    }
    
    private func showEditSheet(for provider: LLMProvider) {
        let editVC = LLMProviderEditViewController(provider: provider)
        editVC.onSave = { [weak self] name, url, model, apiKey in
            self?.delegate?.llmProvidersDidRequestUpdate(
                id: provider.id, name: name, url: url, model: model, apiKey: apiKey
            )
        }
        presentAsSheet(editVC)
    }
}
