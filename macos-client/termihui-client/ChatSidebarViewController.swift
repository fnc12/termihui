import Cocoa
import SnapKit

/// Protocol for AI chat sidebar controller
protocol ChatSidebarViewController: AnyObject {
    var view: NSView { get }
    var sessionId: UInt64 { get set }
    func setInteractive(_ interactive: Bool)
    func focusInputField()
    func startAssistantMessage()
    func appendChunk(_ text: String)
    func finishAssistantMessage()
    func showError(_ error: String)
    func updateProviders(_ newProviders: [LLMProvider])
    func clearMessages()
    func requestProviders()
}

/// Child view controller for the AI chat sidebar
final class ChatSidebarViewControllerImpl: NSViewController, ChatSidebarViewController {
    
    // MARK: - Properties
    weak var delegate: ChatSidebarViewControllerDelegate?
    var sessionId: UInt64 = 0
    
    private var messages: [ChatMessage] = []
    private var providers: [LLMProvider] = []
    private var selectedProviderId: UInt64 = 0
    
    // MARK: - UI Components
    private let visualEffectView = NSVisualEffectView()
    private let titleLabel = NSTextField(labelWithString: "AI Chat")
    private let settingsButton = NSButton()
    private let providerPopUp = NSPopUpButton()
    private let scrollView = NSScrollView()
    private let tableView = NSTableView()
    private let inputContainerView = NSView()
    private let inputTextField = NSTextField()
    private let sendButton = NSButton()
    private let noProvidersView = NSView()
    private let addProviderButton = NSButton()
    
    // MARK: - Lifecycle
    override func loadView() {
        let containerView = ChatSidebarContainerView()
        containerView.wantsLayer = true
        containerView.translatesAutoresizingMaskIntoConstraints = false
        containerView.alphaValue = 0
        containerView.isInteractive = false
        view = containerView
    }
    
    func setInteractive(_ interactive: Bool) {
        (view as? ChatSidebarContainerView)?.isInteractive = interactive
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }
    
    // MARK: - Setup
    private func setupUI() {
        // Visual effect view for native macOS blur
        visualEffectView.translatesAutoresizingMaskIntoConstraints = false
        visualEffectView.material = .sidebar
        visualEffectView.blendingMode = .behindWindow
        visualEffectView.state = .active
        view.addSubview(visualEffectView)
        
        visualEffectView.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }
        
        // Settings button (gear icon) at top right
        settingsButton.translatesAutoresizingMaskIntoConstraints = false
        settingsButton.bezelStyle = .regularSquare
        settingsButton.isBordered = false
        if let image = NSImage(systemSymbolName: "gearshape", accessibilityDescription: "Settings") {
            let config = NSImage.SymbolConfiguration(pointSize: 14, weight: .medium)
            settingsButton.image = image.withSymbolConfiguration(config)
            settingsButton.imagePosition = .imageOnly
            settingsButton.contentTintColor = .secondaryLabelColor
        }
        settingsButton.target = self
        settingsButton.action = #selector(settingsButtonClicked)
        visualEffectView.addSubview(settingsButton)
        
        settingsButton.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(8)
            make.trailing.equalToSuperview().offset(-8)
            make.width.height.equalTo(24)
        }
        
        // Title label at top
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.font = NSFont.systemFont(ofSize: 13, weight: .semibold)
        titleLabel.textColor = .labelColor
        titleLabel.alignment = .left
        visualEffectView.addSubview(titleLabel)
        
        titleLabel.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(12)
            make.leading.equalToSuperview().offset(8)
            make.trailing.equalTo(settingsButton.snp.leading).offset(-8)
        }
        
        // Provider dropdown below title
        providerPopUp.translatesAutoresizingMaskIntoConstraints = false
        providerPopUp.font = NSFont.systemFont(ofSize: 11)
        providerPopUp.controlSize = .small
        providerPopUp.target = self
        providerPopUp.action = #selector(providerSelectionChanged)
        visualEffectView.addSubview(providerPopUp)
        
        providerPopUp.snp.makeConstraints { make in
            make.top.equalTo(titleLabel.snp.bottom).offset(8)
            make.leading.equalToSuperview().offset(8)
            make.trailing.equalToSuperview().offset(-8)
        }
        
        // No providers view (shown when empty)
        setupNoProvidersView()
        visualEffectView.addSubview(noProvidersView)
        
        noProvidersView.snp.makeConstraints { make in
            make.center.equalToSuperview()
            make.width.equalToSuperview().offset(-32)
        }
        
        // Input container at bottom
        setupInputView()
        visualEffectView.addSubview(inputContainerView)
        
        inputContainerView.snp.makeConstraints { make in
            make.leading.trailing.bottom.equalToSuperview()
            make.height.greaterThanOrEqualTo(48)
        }
        
        // Table view for messages
        setupTableView()
        visualEffectView.addSubview(scrollView)
        
        scrollView.snp.makeConstraints { make in
            make.top.equalTo(providerPopUp.snp.bottom).offset(8)
            make.leading.trailing.equalToSuperview()
            make.bottom.equalTo(inputContainerView.snp.top)
        }
        
        // Initial state - hide chat UI if no providers
        updateUIForProviders()
    }
    
    private func setupNoProvidersView() {
        noProvidersView.translatesAutoresizingMaskIntoConstraints = false
        noProvidersView.isHidden = true
        
        let label = NSTextField(labelWithString: "No LLM providers configured")
        label.translatesAutoresizingMaskIntoConstraints = false
        label.font = NSFont.systemFont(ofSize: 13)
        label.textColor = .secondaryLabelColor
        label.alignment = .center
        noProvidersView.addSubview(label)
        
        addProviderButton.translatesAutoresizingMaskIntoConstraints = false
        addProviderButton.title = "Add Provider"
        addProviderButton.bezelStyle = .rounded
        addProviderButton.target = self
        addProviderButton.action = #selector(addProviderButtonClicked)
        noProvidersView.addSubview(addProviderButton)
        
        label.snp.makeConstraints { make in
            make.top.leading.trailing.equalToSuperview()
        }
        
        addProviderButton.snp.makeConstraints { make in
            make.top.equalTo(label.snp.bottom).offset(12)
            make.centerX.equalToSuperview()
            make.bottom.equalToSuperview()
        }
    }
    
    private func updateUIForProviders() {
        let hasProviders = !providers.isEmpty
        let hasMessages = !messages.isEmpty
        
        // Show "no providers" view only if no providers AND no chat history
        noProvidersView.isHidden = hasProviders || hasMessages
        
        // Hide chat UI if no providers and no history
        scrollView.isHidden = !hasProviders && !hasMessages
        inputContainerView.isHidden = !hasProviders
        providerPopUp.isHidden = !hasProviders
    }
    
    private func setupTableView() {
        tableView.headerView = nil
        tableView.rowHeight = 44
        tableView.usesAutomaticRowHeights = true
        tableView.selectionHighlightStyle = .none
        tableView.delegate = self
        tableView.dataSource = self
        tableView.backgroundColor = .clear
        
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("MessageColumn"))
        column.title = "Messages"
        tableView.addTableColumn(column)
        
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.documentView = tableView
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.drawsBackground = false
    }
    
    private func setupInputView() {
        inputContainerView.translatesAutoresizingMaskIntoConstraints = false
        inputContainerView.wantsLayer = true
        inputContainerView.layer?.backgroundColor = NSColor.windowBackgroundColor.withAlphaComponent(0.5).cgColor
        
        // Input text field
        inputTextField.translatesAutoresizingMaskIntoConstraints = false
        inputTextField.placeholderString = "Ask AI..."
        inputTextField.font = NSFont.systemFont(ofSize: 13)
        inputTextField.focusRingType = .none
        inputTextField.isBordered = true
        inputTextField.bezelStyle = .roundedBezel
        inputTextField.target = self
        inputTextField.action = #selector(sendMessage)
        inputContainerView.addSubview(inputTextField)
        
        // Send button
        sendButton.translatesAutoresizingMaskIntoConstraints = false
        sendButton.bezelStyle = .regularSquare
        sendButton.isBordered = false
        if let image = NSImage(systemSymbolName: "arrow.up.circle.fill", accessibilityDescription: "Send") {
            let config = NSImage.SymbolConfiguration(pointSize: 20, weight: .medium)
            sendButton.image = image.withSymbolConfiguration(config)
            sendButton.imagePosition = .imageOnly
            sendButton.contentTintColor = .systemBlue
        }
        sendButton.target = self
        sendButton.action = #selector(sendMessage)
        inputContainerView.addSubview(sendButton)
        
        inputTextField.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(8)
            make.top.equalToSuperview().offset(8)
            make.bottom.equalToSuperview().offset(-8)
            make.trailing.equalTo(sendButton.snp.leading).offset(-8)
        }
        
        sendButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-8)
            make.centerY.equalToSuperview()
            make.width.height.equalTo(28)
        }
    }
    
    // MARK: - Actions
    @objc private func sendMessage() {
        let text = inputTextField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !text.isEmpty else { return }
        guard selectedProviderId != 0 else {
            showError("No LLM provider selected")
            return
        }
        
        // Add user message
        addUserMessage(text)
        
        // Clear input
        inputTextField.stringValue = ""
        
        // Notify delegate
        delegate?.chatSidebarViewController(self, didSendMessage: text, withProviderId: selectedProviderId)
    }
    
    @objc private func settingsButtonClicked() {
        delegate?.chatSidebarViewControllerDidRequestAddProvider(self)
    }
    
    @objc private func addProviderButtonClicked() {
        delegate?.chatSidebarViewControllerDidRequestAddProvider(self)
    }
    
    @objc private func providerSelectionChanged() {
        let index = providerPopUp.indexOfSelectedItem
        guard index >= 0 && index < providers.count else { return }
        selectedProviderId = providers[index].id
        print("📍 Selected provider: \(selectedProviderId) (\(providers[index].name))")
    }
    
    // MARK: - Public API
    
    /// Add a user message to the chat
    func addUserMessage(_ text: String) {
        messages.append(ChatMessage(role: .user, content: text))
        reloadAndScroll()
    }
    
    /// Start streaming assistant response
    func startAssistantMessage() {
        messages.append(ChatMessage(role: .assistant, content: "", isStreaming: true))
        reloadAndScroll()
    }
    
    /// Append chunk to current streaming response
    func appendChunk(_ text: String) {
        guard !messages.isEmpty else { return }
        let lastIndex = messages.count - 1
        guard messages[lastIndex].isStreaming else { return }
        
        messages[lastIndex].content += text
        
        // Update only last row
        tableView.reloadData(forRowIndexes: IndexSet(integer: lastIndex),
                            columnIndexes: IndexSet(integer: 0))
        scrollToBottom()
    }
    
    /// Finish streaming response
    func finishAssistantMessage() {
        guard !messages.isEmpty else { return }
        let lastIndex = messages.count - 1
        messages[lastIndex].isStreaming = false
        
        tableView.reloadData(forRowIndexes: IndexSet(integer: lastIndex),
                            columnIndexes: IndexSet(integer: 0))
    }
    
    /// Show error message
    func showError(_ error: String) {
        // If streaming, finish with error
        if !messages.isEmpty && messages[messages.count - 1].isStreaming {
            messages[messages.count - 1].content += "\n[Error: \(error)]"
            messages[messages.count - 1].isStreaming = false
        } else {
            messages.append(ChatMessage(role: .assistant, content: "[Error: \(error)]"))
        }
        reloadAndScroll()
    }
    
    /// Clear all messages
    func clearMessages() {
        messages.removeAll()
        tableView.reloadData()
    }
    
    /// Update LLM providers list
    func updateProviders(_ newProviders: [LLMProvider]) {
        providers = newProviders
        
        // Update popup
        providerPopUp.removeAllItems()
        for provider in providers {
            providerPopUp.addItem(withTitle: provider.name)
        }
        
        // Select first provider or keep current selection
        if !providers.isEmpty {
            if let currentIndex = providers.firstIndex(where: { $0.id == selectedProviderId }) {
                providerPopUp.selectItem(at: currentIndex)
            } else {
                selectedProviderId = providers[0].id
                providerPopUp.selectItem(at: 0)
            }
        } else {
            selectedProviderId = 0
        }
        
        updateUIForProviders()
    }
    
    /// Request providers list from server
    func requestProviders() {
        delegate?.chatSidebarViewControllerDidRequestProviders(self)
    }
    
    /// Get current selected provider ID
    var currentProviderId: UInt64 {
        return selectedProviderId
    }
    
    /// Focus the input text field
    func focusInputField() {
        view.window?.makeFirstResponder(inputTextField)
    }
    
    // MARK: - Helpers
    private func reloadAndScroll() {
        tableView.reloadData()
        scrollToBottom()
    }
    
    private func scrollToBottom() {
        guard !messages.isEmpty else { return }
        tableView.scrollRowToVisible(messages.count - 1)
    }
    
    // MARK: - Accessors for TableView extension
    var messagesCount: Int {
        return messages.count
    }
    
    func getMessage(at index: Int) -> ChatMessage {
        return messages[index]
    }
}
