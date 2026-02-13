import UIKit
import SnapKit

protocol ChatViewControllerDelegate: AnyObject {
    func chatViewController(_ controller: ChatViewController, didSendMessage message: String, withProviderId providerId: UInt64)
    func chatViewControllerDidRequestProviders(_ controller: ChatViewController)
    func chatViewControllerDidRequestManageProviders(_ controller: ChatViewController)
    func chatViewControllerDidRequestChatHistory(_ controller: ChatViewController, forSession sessionId: UInt64)
    func chatViewControllerDidClose(_ controller: ChatViewController)
}

/// AI Chat screen for iOS
class ChatViewController: UIViewController {
    
    // MARK: - Properties
    var sessionId: UInt64 = 0
    weak var delegate: ChatViewControllerDelegate?
    
    private var messages: [ChatMessage] = []
    private var providers: [LLMProvider] = []
    private var selectedProviderId: UInt64 = 0
    private var isLoadingHistory = false
    private var historyLoadedForSession: UInt64 = 0
    
    // MARK: - UI Components
    private let providerButton = UIButton(type: .system)
    private let tableView = UITableView(frame: .zero, style: .plain)
    private let inputContainerView = UIView()
    private let inputTextField = UITextField()
    private let sendButton = UIButton(type: .system)
    private let loadingIndicator = UIActivityIndicatorView(style: .medium)
    private let noProvidersView = UIView()
    
    private var keyboardHeight: CGFloat = 0
    
    // MARK: - Lifecycle
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupKeyboardObservers()
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        
        // Request providers and history on appear
        delegate?.chatViewControllerDidRequestProviders(self)
        requestChatHistory()
        
        // Reload table to show current state (including streaming messages)
        tableView.reloadData()
        if !messages.isEmpty {
            scrollToBottom()
        }
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        
        if isMovingFromParent {
            delegate?.chatViewControllerDidClose(self)
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        title = "AI Chat"
        view.backgroundColor = .systemBackground
        
        // Use standard (not large) title
        navigationItem.largeTitleDisplayMode = .never
        
        // Right bar buttons: settings + terminal
        let settingsButton = UIBarButtonItem(
            image: UIImage(systemName: "gearshape"),
            style: .plain,
            target: self,
            action: #selector(settingsTapped)
        )
        let terminalButton = UIBarButtonItem(
            image: UIImage(systemName: "terminal"),
            style: .plain,
            target: self,
            action: #selector(backToTerminalTapped)
        )
        navigationItem.rightBarButtonItems = [terminalButton, settingsButton]
        
        // Provider button (tappable header below nav bar)
        setupProviderButton()
        
        // Table view for messages
        setupTableView()
        
        // Input container at bottom
        setupInputView()
        
        // No providers view
        setupNoProvidersView()
        
        // Loading indicator
        loadingIndicator.hidesWhenStopped = true
        view.addSubview(loadingIndicator)
        loadingIndicator.snp.makeConstraints { make in
            make.center.equalToSuperview()
        }
        
        // Initial state
        updateUIForProviders()
    }
    
    private func setupProviderButton() {
        providerButton.setTitle("Select Provider ▾", for: .normal)
        providerButton.titleLabel?.font = .systemFont(ofSize: 15, weight: .medium)
        providerButton.contentHorizontalAlignment = .center
        providerButton.backgroundColor = .secondarySystemBackground
        providerButton.layer.cornerRadius = 8
        providerButton.addTarget(self, action: #selector(providerButtonTapped), for: .touchUpInside)
        
        view.addSubview(providerButton)
        providerButton.snp.makeConstraints { make in
            make.top.equalTo(view.safeAreaLayoutGuide).offset(8)
            make.leading.equalToSuperview().offset(16)
            make.trailing.equalToSuperview().offset(-16)
            make.height.equalTo(40)
        }
    }
    
    private func setupTableView() {
        tableView.backgroundColor = .systemBackground
        tableView.separatorStyle = .none
        tableView.delegate = self
        tableView.dataSource = self
        tableView.register(ChatMessageCell.self, forCellReuseIdentifier: ChatMessageCell.reuseIdentifier)
        tableView.rowHeight = UITableView.automaticDimension
        tableView.estimatedRowHeight = 60
        tableView.keyboardDismissMode = .interactive
        
        view.addSubview(tableView)
        tableView.snp.makeConstraints { make in
            make.top.equalTo(providerButton.snp.bottom).offset(8)
            make.leading.trailing.equalToSuperview()
        }
    }
    
    private func setupInputView() {
        inputContainerView.backgroundColor = .secondarySystemBackground
        view.addSubview(inputContainerView)
        
        inputTextField.placeholder = "Ask AI..."
        inputTextField.font = .systemFont(ofSize: 16)
        inputTextField.borderStyle = .roundedRect
        inputTextField.returnKeyType = .send
        inputTextField.delegate = self
        inputContainerView.addSubview(inputTextField)
        
        sendButton.setImage(UIImage(systemName: "arrow.up.circle.fill"), for: .normal)
        sendButton.tintColor = .systemBlue
        sendButton.addTarget(self, action: #selector(sendButtonTapped), for: .touchUpInside)
        inputContainerView.addSubview(sendButton)
        
        inputContainerView.snp.makeConstraints { make in
            make.top.equalTo(tableView.snp.bottom)
            make.leading.trailing.equalToSuperview()
            make.bottom.equalTo(view.safeAreaLayoutGuide)
        }
        
        inputTextField.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.top.equalToSuperview().offset(8)
            make.bottom.equalToSuperview().offset(-8)
            make.trailing.equalTo(sendButton.snp.leading).offset(-8)
            make.height.greaterThanOrEqualTo(36)
        }
        
        sendButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-12)
            make.centerY.equalTo(inputTextField)
            make.width.height.equalTo(36)
        }
    }
    
    private func setupNoProvidersView() {
        noProvidersView.isHidden = true
        view.addSubview(noProvidersView)
        
        let label = UILabel()
        label.text = "No LLM providers configured"
        label.textColor = .secondaryLabel
        label.font = .systemFont(ofSize: 15)
        label.textAlignment = .center
        noProvidersView.addSubview(label)
        
        let infoLabel = UILabel()
        infoLabel.text = "Configure providers on the server"
        infoLabel.textColor = .tertiaryLabel
        infoLabel.font = .systemFont(ofSize: 13)
        infoLabel.textAlignment = .center
        noProvidersView.addSubview(infoLabel)
        
        noProvidersView.snp.makeConstraints { make in
            make.center.equalToSuperview()
            make.width.equalToSuperview().offset(-32)
        }
        
        label.snp.makeConstraints { make in
            make.top.leading.trailing.equalToSuperview()
        }
        
        infoLabel.snp.makeConstraints { make in
            make.top.equalTo(label.snp.bottom).offset(4)
            make.leading.trailing.bottom.equalToSuperview()
        }
    }
    
    private func setupKeyboardObservers() {
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(keyboardWillShow),
            name: UIResponder.keyboardWillShowNotification,
            object: nil
        )
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(keyboardWillHide),
            name: UIResponder.keyboardWillHideNotification,
            object: nil
        )
    }
    
    // MARK: - Keyboard Handling
    
    @objc private func keyboardWillShow(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let keyboardFrame = userInfo[UIResponder.keyboardFrameEndUserInfoKey] as? CGRect,
              let duration = userInfo[UIResponder.keyboardAnimationDurationUserInfoKey] as? Double else {
            return
        }
        
        keyboardHeight = keyboardFrame.height
        
        UIView.animate(withDuration: duration) {
            self.inputContainerView.snp.updateConstraints { make in
                make.bottom.equalTo(self.view.safeAreaLayoutGuide).offset(-(self.keyboardHeight - self.view.safeAreaInsets.bottom))
            }
            self.view.layoutIfNeeded()
        } completion: { _ in
            self.scrollToBottom()
        }
    }
    
    @objc private func keyboardWillHide(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let duration = userInfo[UIResponder.keyboardAnimationDurationUserInfoKey] as? Double else {
            return
        }
        
        keyboardHeight = 0
        
        UIView.animate(withDuration: duration) {
            self.inputContainerView.snp.updateConstraints { make in
                make.bottom.equalTo(self.view.safeAreaLayoutGuide)
            }
            self.view.layoutIfNeeded()
        }
    }
    
    // MARK: - Actions
    
    @objc private func backToTerminalTapped() {
        navigationController?.popViewController(animated: true)
    }
    
    @objc private func settingsTapped() {
        delegate?.chatViewControllerDidRequestManageProviders(self)
    }
    
    @objc private func providerButtonTapped() {
        guard !providers.isEmpty else { return }
        
        let alert = UIAlertController(title: "Select Provider", message: nil, preferredStyle: .actionSheet)
        
        for provider in providers {
            let action = UIAlertAction(title: provider.name, style: .default) { [weak self] _ in
                self?.selectedProviderId = provider.id
                self?.updateProviderButtonTitle()
            }
            if provider.id == selectedProviderId {
                action.setValue(true, forKey: "checked")
            }
            alert.addAction(action)
        }
        
        alert.addAction(UIAlertAction(title: "Cancel", style: .cancel))
        
        // For iPad
        if let popover = alert.popoverPresentationController {
            popover.sourceView = providerButton
            popover.sourceRect = providerButton.bounds
        }
        
        present(alert, animated: true)
    }
    
    @objc private func sendButtonTapped() {
        sendCurrentMessage()
    }
    
    // MARK: - Public API
    
    /// Current list of providers
    var currentProviders: [LLMProvider] {
        return providers
    }
    
    /// Update providers list
    func updateProviders(_ newProviders: [LLMProvider]) {
        print("📱 ChatViewController.updateProviders: \(newProviders.count) providers")
        providers = newProviders
        
        // Select first provider if none selected
        if !providers.isEmpty {
            if let currentIndex = providers.firstIndex(where: { $0.id == selectedProviderId }) {
                // Keep current selection
                selectedProviderId = providers[currentIndex].id
            } else {
                selectedProviderId = providers[0].id
            }
        } else {
            selectedProviderId = 0
        }
        
        updateProviderButtonTitle()
        updateUIForProviders()
    }
    
    /// Request chat history
    func requestChatHistory() {
        guard sessionId != 0 else { return }
        guard historyLoadedForSession != sessionId else { return }
        
        setLoading(true)
        delegate?.chatViewControllerDidRequestChatHistory(self, forSession: sessionId)
    }
    
    /// Load chat history from server
    func loadChatHistory(_ historyMessages: [ChatMessageInfo]) {
        setLoading(false)
        historyLoadedForSession = sessionId
        
        messages.removeAll()
        for msg in historyMessages {
            let role: ChatMessage.Role
            switch msg.role {
            case "user": role = .user
            case "error": role = .error
            default: role = .assistant
            }
            messages.append(ChatMessage(role: role, content: msg.content))
        }
        
        tableView.reloadData()
        scrollToBottom()
        updateUIForProviders()
    }
    
    /// Add user message
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
        
        let indexPath = IndexPath(row: lastIndex, section: 0)
        
        // Update cell directly if visible (avoids jarring reloads during streaming)
        if let cell = tableView.cellForRow(at: indexPath) as? ChatMessageCell {
            cell.updateText(messages[lastIndex].content)
            
            // Recalculate row heights without animation
            UIView.performWithoutAnimation {
                tableView.beginUpdates()
                tableView.endUpdates()
            }
        } else {
            // Cell not visible — fallback to reload
            UIView.performWithoutAnimation {
                tableView.reloadRows(at: [indexPath], with: .none)
            }
        }
        
        scrollToBottom(animated: false)
    }
    
    /// Finish streaming response
    func finishAssistantMessage() {
        guard !messages.isEmpty else { return }
        let lastIndex = messages.count - 1
        messages[lastIndex].isStreaming = false
        
        let indexPath = IndexPath(row: lastIndex, section: 0)
        tableView.reloadRows(at: [indexPath], with: .none)
    }
    
    /// Show error
    func showError(_ error: String) {
        // Remove empty streaming message if exists
        if !messages.isEmpty && messages[messages.count - 1].isStreaming && messages[messages.count - 1].content.isEmpty {
            messages.removeLast()
        }
        
        messages.append(ChatMessage(role: .error, content: error))
        reloadAndScroll()
    }
    
    /// Set loading state
    func setLoading(_ loading: Bool) {
        isLoadingHistory = loading
        if loading {
            loadingIndicator.startAnimating()
            tableView.isHidden = true
            noProvidersView.isHidden = true
        } else {
            loadingIndicator.stopAnimating()
            updateUIForProviders()
        }
    }
    
    // MARK: - Helpers
    
    private func updateProviderButtonTitle() {
        if let provider = providers.first(where: { $0.id == selectedProviderId }) {
            providerButton.setTitle("\(provider.name) ▾", for: .normal)
        } else if providers.isEmpty {
            providerButton.setTitle("No Providers", for: .normal)
        } else {
            providerButton.setTitle("Select Provider ▾", for: .normal)
        }
    }
    
    private func updateUIForProviders() {
        let hasProviders = !providers.isEmpty
        let hasMessages = !messages.isEmpty
        
        noProvidersView.isHidden = hasProviders || hasMessages
        tableView.isHidden = !hasProviders && !hasMessages
        inputContainerView.isHidden = !hasProviders
        providerButton.isEnabled = hasProviders
    }
    
    private func reloadAndScroll() {
        tableView.reloadData()
        scrollToBottom()
    }
    
    private func scrollToBottom(animated: Bool = true) {
        guard !messages.isEmpty else { return }
        let indexPath = IndexPath(row: messages.count - 1, section: 0)
        tableView.scrollToRow(at: indexPath, at: .bottom, animated: animated)
    }
    
    // MARK: - Accessors for TableView Extension
    
    var messagesCount: Int {
        return messages.count
    }
    
    func getMessage(at index: Int) -> ChatMessage {
        return messages[index]
    }
    
    func sendCurrentMessage() {
        guard let text = inputTextField.text?.trimmingCharacters(in: .whitespacesAndNewlines),
              !text.isEmpty else { return }
        
        guard selectedProviderId != 0 else {
            showError("No LLM provider selected")
            return
        }
        
        // Add user message to UI
        addUserMessage(text)
        
        // Clear input
        inputTextField.text = ""
        
        // Send to server
        delegate?.chatViewController(self, didSendMessage: text, withProviderId: selectedProviderId)
    }
}
