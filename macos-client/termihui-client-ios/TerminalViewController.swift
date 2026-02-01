import UIKit
import SnapKit

protocol TerminalViewControllerDelegate: AnyObject {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String)
    func terminalViewControllerDidClose(_ controller: TerminalViewController)
    func terminalViewControllerDidRequestChat(_ controller: TerminalViewController)
}

/// Terminal session screen for iOS with command blocks
class TerminalViewController: UIViewController {
    
    // MARK: - UI
    private let tableView = UITableView(frame: .zero, style: .plain)
    private let inputContainerView = UIView()
    private let cwdLabel = UILabel()
    private let inputTextField = UITextField()
    private let sendButton = UIButton(type: .system)
    
    // MARK: - Properties
    var sessionId: UInt64 = 0 {
        didSet {
            updateSessionTitle()
        }
    }
    weak var delegate: TerminalViewControllerDelegate?
    
    private var commandBlocks: [CommandBlockModel] = []
    private var currentBlockIndex: Int?
    private var keyboardHeight: CGFloat = 0
    
    /// Server home directory (for path shortening)
    var serverHome: String = ""
    
    /// Current working directory
    private var currentCwd: String = ""
    
    // MARK: - Lifecycle
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupKeyboardObservers()
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        
        if isMovingFromParent {
            delegate?.terminalViewControllerDidClose(self)
        }
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        updateSessionTitle()
        
        // Use standard (not large) title for terminal screen
        navigationItem.largeTitleDisplayMode = .never
        
        // Chat button in nav bar (right side)
        let chatButton = UIBarButtonItem(
            image: UIImage(systemName: "bubble.left.and.bubble.right"),
            style: .plain,
            target: self,
            action: #selector(chatButtonTapped)
        )
        navigationItem.rightBarButtonItem = chatButton
        
        view.backgroundColor = .systemBackground
        
        // Table view for command blocks
        tableView.backgroundColor = .systemBackground
        tableView.separatorStyle = .singleLine
        tableView.separatorColor = .separator
        tableView.separatorInset = UIEdgeInsets(top: 0, left: 20, bottom: 0, right: 0)
        tableView.delegate = self
        tableView.dataSource = self
        tableView.register(CommandBlockCell.self, forCellReuseIdentifier: CommandBlockCell.reuseIdentifier)
        tableView.rowHeight = UITableView.automaticDimension
        tableView.estimatedRowHeight = 60
        tableView.keyboardDismissMode = .interactive
        
        // Input container
        inputContainerView.backgroundColor = .secondarySystemBackground
        
        // CWD label — purple, like in Warp
        cwdLabel.font = UIFont.monospacedSystemFont(ofSize: 11, weight: .medium)
        cwdLabel.textColor = UIColor(red: 0.6, green: 0.4, blue: 0.9, alpha: 1.0)
        cwdLabel.text = "~"
        cwdLabel.lineBreakMode = .byTruncatingHead
        
        // Input text field
        inputTextField.placeholder = "Enter command..."
        inputTextField.font = UIFont.monospacedSystemFont(ofSize: 16, weight: .regular)
        inputTextField.borderStyle = .roundedRect
        inputTextField.autocorrectionType = .no
        inputTextField.autocapitalizationType = .none
        inputTextField.returnKeyType = .send
        inputTextField.delegate = self
        
        // Send button
        sendButton.setImage(UIImage(systemName: "arrow.up.circle.fill"), for: .normal)
        sendButton.tintColor = .systemBlue
        sendButton.addTarget(self, action: #selector(sendButtonTapped), for: .touchUpInside)
        
        // Layout
        view.addSubview(tableView)
        view.addSubview(inputContainerView)
        inputContainerView.addSubview(cwdLabel)
        inputContainerView.addSubview(inputTextField)
        inputContainerView.addSubview(sendButton)
        
        tableView.snp.makeConstraints { make in
            make.top.leading.trailing.equalTo(view.safeAreaLayoutGuide)
            make.bottom.equalTo(inputContainerView.snp.top)
        }
        
        inputContainerView.snp.makeConstraints { make in
            make.leading.trailing.equalToSuperview()
            make.bottom.equalTo(view.safeAreaLayoutGuide)
        }
        
        cwdLabel.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.trailing.equalToSuperview().offset(-12)
            make.top.equalToSuperview().offset(6)
            make.height.equalTo(16)
        }
        
        inputTextField.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.top.equalTo(cwdLabel.snp.bottom).offset(4)
            make.trailing.equalTo(sendButton.snp.leading).offset(-8)
            make.bottom.equalToSuperview().offset(-12)
            make.height.greaterThanOrEqualTo(36)
        }
        
        sendButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-12)
            make.centerY.equalTo(inputTextField)
            make.width.height.equalTo(36)
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
        
        UIView.animate(withDuration: duration, animations: {
            self.inputContainerView.snp.updateConstraints { make in
                make.bottom.equalTo(self.view.safeAreaLayoutGuide).offset(-(self.keyboardHeight - self.view.safeAreaInsets.bottom))
            }
            self.view.layoutIfNeeded()
        }, completion: { _ in
            self.scrollToBottom()
        })
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
    
    @objc private func sendButtonTapped() {
        sendCurrentCommand()
    }
    
    @objc private func chatButtonTapped() {
        delegate?.terminalViewControllerDidRequestChat(self)
    }
    
    private func sendCurrentCommand() {
        guard let command = inputTextField.text, !command.isEmpty else { return }
        
        // Send to server
        delegate?.terminalViewController(self, didSendCommand: command)
        
        // Clear input
        inputTextField.text = ""
    }
    
    // MARK: - Public Methods
    
    /// Load history from server
    func loadHistory(_ history: [CommandHistoryRecordShared]) {
        commandBlocks = history.map { CommandBlockModel(from: $0) }
        currentBlockIndex = commandBlocks.isEmpty ? nil : commandBlocks.count - 1
        
        tableView.reloadData()
        updateContentInset()
        scrollToBottom()
    }
    
    /// Called when a new command starts
    func didStartCommandBlock(command: String?, cwd: String?) {
        let block = CommandBlockModel(
            id: UInt64(commandBlocks.count + 1),
            command: command,
            segments: [],
            isFinished: false,
            exitCode: 0,
            cwdStart: cwd
        )
        commandBlocks.append(block)
        currentBlockIndex = commandBlocks.count - 1
        
        let indexPath = IndexPath(row: commandBlocks.count - 1, section: 0)
        tableView.performBatchUpdates({
            self.tableView.insertRows(at: [indexPath], with: .automatic)
        }, completion: { _ in
            self.updateContentInset()
            self.scrollToBottom()
        })
    }
    
    /// Called when command finishes
    func didFinishCommandBlock(exitCode: Int, cwd: String?) {
        guard let index = currentBlockIndex else { return }
        
        commandBlocks[index].isFinished = true
        commandBlocks[index].exitCode = exitCode
        commandBlocks[index].cwdEnd = cwd
        
        let indexPath = IndexPath(row: index, section: 0)
        tableView.reloadRows(at: [indexPath], with: .none)
    }
    
    /// Append output to current command block
    func appendStyledOutput(_ segments: [StyledSegmentShared]) {
        guard !segments.isEmpty else { return }
        
        // If no current block, create one
        if currentBlockIndex == nil {
            let block = CommandBlockModel(
                id: UInt64(commandBlocks.count + 1),
                segments: segments
            )
            commandBlocks.append(block)
            currentBlockIndex = commandBlocks.count - 1
            
            let indexPath = IndexPath(row: commandBlocks.count - 1, section: 0)
            tableView.performBatchUpdates({
                self.tableView.insertRows(at: [indexPath], with: .none)
            }, completion: { _ in
                self.updateContentInset()
                self.scrollToBottom()
            })
        } else {
            // Append to current block
            let index = currentBlockIndex!
            commandBlocks[index].segments.append(contentsOf: segments)
            
            let indexPath = IndexPath(row: index, section: 0)
            tableView.performBatchUpdates({
                self.tableView.reloadRows(at: [indexPath], with: .none)
            }, completion: { _ in
                self.updateContentInset()
                self.scrollToBottom()
            })
        }
    }
    
    /// Append raw text output
    func appendOutput(_ text: String) {
        let style = SegmentStyleShared()
        let segment = StyledSegmentShared(text: text, style: style)
        appendStyledOutput([segment])
    }
    
    /// Clear all output
    func clearOutput() {
        commandBlocks.removeAll()
        currentBlockIndex = nil
        tableView.reloadData()
    }
    
    /// Updates current working directory display
    func updateCurrentCwd(_ cwd: String) {
        currentCwd = cwd
        // Shorten path if server provided home directory
        let displayCwd: String
        if !serverHome.isEmpty && cwd.hasPrefix(serverHome) {
            displayCwd = "~" + String(cwd.dropFirst(serverHome.count))
        } else {
            displayCwd = cwd
        }
        cwdLabel.text = displayCwd
    }
    
    /// Updates navigation bar title with session ID
    private func updateSessionTitle() {
        if sessionId > 0 {
            title = "#\(sessionId)"
            print("📱 Title set to: #\(sessionId)")
        } else {
            title = "Terminal"
            print("📱 Title set to: Terminal (sessionId=0)")
        }
    }
    
    // MARK: - Helpers
    
    private func scrollToBottom() {
        guard !commandBlocks.isEmpty else { return }
        let indexPath = IndexPath(row: commandBlocks.count - 1, section: 0)
        tableView.scrollToRow(at: indexPath, at: .bottom, animated: true)
    }
    
    /// Adjust content inset to pin content to bottom when smaller than viewport
    private func updateContentInset() {
        tableView.layoutIfNeeded()
        
        let contentHeight = tableView.contentSize.height
        let viewHeight = tableView.bounds.height
        
        if contentHeight < viewHeight {
            tableView.contentInset = UIEdgeInsets(top: viewHeight - contentHeight, left: 0, bottom: 0, right: 0)
        } else {
            tableView.contentInset = .zero
        }
    }
    
    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        updateContentInset()
    }
}

// MARK: - UITableViewDataSource

extension TerminalViewController: UITableViewDataSource {
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return commandBlocks.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: CommandBlockCell.reuseIdentifier, for: indexPath) as! CommandBlockCell
        cell.configure(with: commandBlocks[indexPath.row], serverHome: serverHome)
        return cell
    }
}

// MARK: - UITableViewDelegate

extension TerminalViewController: UITableViewDelegate {
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        // Could add copy functionality here
    }
}

// MARK: - UITextFieldDelegate

extension TerminalViewController: UITextFieldDelegate {
    func textFieldShouldReturn(_ textField: UITextField) -> Bool {
        sendCurrentCommand()
        return false
    }
}
