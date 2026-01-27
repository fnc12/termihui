import Cocoa
import SnapKit

/// Main terminal screen
class TerminalViewController: NSViewController, NSGestureRecognizerDelegate {
    
    // MARK: - UI Components
    private let topToolbarView = NSView()
    private let hamburgerButton = NSButton()
    private let chatButton = NSButton()
    private let sessionLabel = NSTextField(labelWithString: "")
    
    let terminalScrollView = NSScrollView()
    var collectionView = NSCollectionView()
    let collectionLayout = NSCollectionViewFlowLayout()
    
    private let inputContainerView = NSView()
    private let cwdLabel = NSTextField(labelWithString: "")
    let commandTextField = TabHandlingTextField()
    private let sendButton = NSButton(title: "Send", target: nil, action: nil)
    private var inputUnderlineView: NSView!
    
    // Session sidebar (lazy - created on first toggle)
    var sessionListController: SessionListViewController?
    private var isSidebarVisible = false
    
    // Chat sidebar (controller created lazily on first access, view setup on first toggle)
    lazy var chatSidebarController: ChatSidebarViewController = {
        let controller = ChatSidebarViewController()
        controller.delegate = self
        addChild(controller)
        return controller
    }()
    private var isChatSidebarVisible = false
    
    // Cached session data (applied when sidebar is created)
    var cachedSessions: [SessionInfo] = []
    var cachedActiveSessionId: UInt64 = 0
    
    // Current working directory and server home
    private var currentCwd: String = ""
    var serverHome: String = ""
    
    // MARK: - Properties
    weak var delegate: TerminalViewControllerDelegate?
    private var serverAddress: String = ""
    private let baseTopInset: CGFloat = 16
    
    /// Client core instance for C++ functionality
    var clientCore: ClientCoreWrapper?
    
    // Terminal size tracking
    private var lastTerminalCols: Int = 0
    private var lastTerminalRows: Int = 0
    private var resizeDebounceTimer: Timer?
    private let terminalFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
    
    // MARK: - Command Blocks Model (in-memory only, no UI yet)
    struct CommandBlock {
        var commandId: UInt64?  // Server command ID, nil for currently executing command
        var command: String?
        var outputSegments: [StyledSegment]  // Pre-parsed styled segments from C++ core
        var isFinished: Bool
        var exitCode: Int?
        var cwdStart: String?   // cwd when command started
        var cwdEnd: String?     // cwd after command finished (can change, e.g. cd)
        
        /// Plain text output (for history, search, etc.)
        var outputText: String {
            outputSegments.map { $0.text }.joined()
        }
    }
    var commandBlocks: [CommandBlock] = []
    
    // Pointer to current unfinished block (array index)
    var currentBlockIndex: Int? = nil

    // MARK: - Global Document for unified selection (model only)
    enum SegmentKind { case header, output }
    struct GlobalSegment {
        let blockIndex: Int
        let kind: SegmentKind
        var range: NSRange // global range in combined document
    }
    struct GlobalDocument {
        var totalLength: Int = 0
        var segments: [GlobalSegment] = []
    }
    var globalDocument = GlobalDocument()

    // MARK: - Selection state (global)
    var isSelecting: Bool = false
    var selectionAnchor: Int? = nil // global index of selection start
    var selectionRange: NSRange? = nil // current global range
    
    // MARK: - Autoscroll state
    var autoscrollTimer: Timer?
    var lastDragEvent: NSEvent?
    
    // MARK: - Raw Input Mode (when command is running)
    var isCommandRunning: Bool = false
    
    // MARK: - Interactive Mode (full terminal emulation)
    var isInteractiveMode: Bool = false
    private var interactiveTextView: NSTextView?
    private var interactiveScrollView: NSScrollView?
    private var interactiveScreenRows: Int = 24
    private var interactiveScreenColumns: Int = 80
    private var interactiveScreenLines: [[StyledSegment]] = []  // Current screen content
    private var interactiveCursorRow: Int = 0
    private var interactiveCursorColumn: Int = 0
    
    override func loadView() {
        view = NSView()
        view.wantsLayer = true
        // NOTE: Don't disable translatesAutoresizingMaskIntoConstraints for root view
        // because parent ViewController uses frame-based layout for child view controllers
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupLayout()
        setupActions()
    }
    
    override var acceptsFirstResponder: Bool {
        return true
    }
    
    override func viewDidAppear() {
        super.viewDidAppear()
        
        // Force update parent view layout
        view.superview?.layoutSubtreeIfNeeded()
        view.layoutSubtreeIfNeeded()
        
        // Small delay to ensure layout is complete
        DispatchQueue.main.async {
            // RECREATE NSTextView AFTER layout is complete
            self.recreateTextViewWithCorrectSize()
            
            // Set focus on command input field
            self.view.window?.makeFirstResponder(self.commandTextField)
        }
    }
    
    override func viewDidLayout() {
        super.viewDidLayout()
        
        // Update NSTextView frame when view size changes
        updateTextViewFrame()
        
        // Update sidebar transforms when view size changes
        updateSidebarTransform()
        updateChatSidebarTransform()
        
        // Debounced terminal resize notification
        scheduleTerminalResizeUpdate()
    }
    
    /// Calculate terminal size in characters based on view dimensions
    private func calculateTerminalSize() -> (cols: Int, rows: Int) {
        // Use visible bounds, not content size
        var viewWidth = collectionView.bounds.width
        let viewHeight = terminalScrollView.bounds.height
        
        // Account for vertical scroller if it's not overlay style
        if terminalScrollView.scrollerStyle == .legacy,
           let scroller = terminalScrollView.verticalScroller, !scroller.isHidden {
            viewWidth -= scroller.frame.width
        }
        
        // Account for all padding: sectionInset + CommandBlockItem padding
        let sectionInsets = collectionLayout.sectionInset.left + collectionLayout.sectionInset.right
        let blockPadding = CommandBlockItem.horizontalPadding * 2
        let effectiveWidth = viewWidth - sectionInsets - blockPadding
        
        // Calculate character dimensions (use same method as CommandBlockItem for consistency)
        let charWidth = "M".size(withAttributes: [.font: terminalFont]).width
        let lineHeight = ceil(terminalFont.ascender - terminalFont.descender + terminalFont.leading)
        
        // Calculate columns and rows
        let cols = max(20, Int(floor(effectiveWidth / charWidth)))
        let rows = max(5, Int(floor(viewHeight / lineHeight)))
        
        return (cols, rows)
    }
    
    /// Schedule a debounced terminal resize update
    private func scheduleTerminalResizeUpdate() {
        resizeDebounceTimer?.invalidate()
        resizeDebounceTimer = Timer.scheduledTimer(withTimeInterval: 0.15, repeats: false) { [weak self] _ in
            self?.sendTerminalResizeIfNeeded()
        }
    }
    
    /// Send terminal resize to server if dimensions changed
    private func sendTerminalResizeIfNeeded() {
        let (cols, rows) = calculateTerminalSize()
        
        // Only send if size actually changed
        if cols != lastTerminalCols || rows != lastTerminalRows {
            lastTerminalCols = cols
            lastTerminalRows = rows
            
            // print("📐 Terminal size changed: \(cols)x\(rows)")
            clientCore?.send(["type": "resize", "cols": cols, "rows": rows])
        }
    }
    
    /// Force send current terminal size (e.g., on initial connection)
    func sendInitialTerminalSize() {
        let (cols, rows) = calculateTerminalSize()
        lastTerminalCols = cols
        lastTerminalRows = rows
        
        // print("📐 Initial terminal size: \(cols)x\(rows)")
        clientCore?.send(["type": "resize", "cols": cols, "rows": rows])
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
        // IMPORTANT: Disable autoresizing mask BEFORE adding to view hierarchy
        topToolbarView.translatesAutoresizingMaskIntoConstraints = false
        terminalScrollView.translatesAutoresizingMaskIntoConstraints = false
        inputContainerView.translatesAutoresizingMaskIntoConstraints = false
        
        setupToolbarView()
        setupTerminalView()
        setupInputView()
        
        // Add main views
        view.addSubview(terminalScrollView)
        view.addSubview(inputContainerView)
        view.addSubview(topToolbarView) // Toolbar on top (last = front)
        
        // NOTE: Sidebar is created lazily on first toggle to prevent flash on startup
    }
    
    private func setupToolbarView() {
        topToolbarView.wantsLayer = true
        topToolbarView.layer?.backgroundColor = NSColor(white: 0.15, alpha: 1.0).cgColor
        
        // Disable autoresizing for subviews
        hamburgerButton.translatesAutoresizingMaskIntoConstraints = false
        chatButton.translatesAutoresizingMaskIntoConstraints = false
        sessionLabel.translatesAutoresizingMaskIntoConstraints = false
        
        // Hamburger button (menu icon) - left side
        hamburgerButton.bezelStyle = .regularSquare
        hamburgerButton.isBordered = false
        if let image = NSImage(systemSymbolName: "line.3.horizontal", accessibilityDescription: "Menu") {
            let config = NSImage.SymbolConfiguration(pointSize: 16, weight: .medium)
            hamburgerButton.image = image.withSymbolConfiguration(config)
            hamburgerButton.imagePosition = .imageOnly
            hamburgerButton.contentTintColor = .white
        } else {
            // Fallback if SF Symbol not available
            hamburgerButton.title = "≡"
            hamburgerButton.font = NSFont.systemFont(ofSize: 20, weight: .medium)
        }
        hamburgerButton.target = self
        hamburgerButton.action = #selector(toggleSidebar)
        topToolbarView.addSubview(hamburgerButton)
        
        // Chat button (AI icon) - right side
        chatButton.bezelStyle = .regularSquare
        chatButton.isBordered = false
        if let image = NSImage(systemSymbolName: "bubble.left.and.bubble.right", accessibilityDescription: "AI Chat") {
            let config = NSImage.SymbolConfiguration(pointSize: 16, weight: .medium)
            chatButton.image = image.withSymbolConfiguration(config)
            chatButton.imagePosition = .imageOnly
            chatButton.contentTintColor = .white
        } else {
            chatButton.title = "AI"
            chatButton.font = NSFont.systemFont(ofSize: 12, weight: .medium)
        }
        chatButton.target = self
        chatButton.action = #selector(toggleChatSidebar)
        topToolbarView.addSubview(chatButton)
        
        // Session label (centered)
        sessionLabel.font = NSFont.systemFont(ofSize: 13, weight: .medium)
        sessionLabel.textColor = .white
        sessionLabel.alignment = .center
        sessionLabel.backgroundColor = .clear
        sessionLabel.isBordered = false
        sessionLabel.isEditable = false
        sessionLabel.isSelectable = false
        topToolbarView.addSubview(sessionLabel)
    }
    
    /// Creates sidebar lazily - called on first toggle
    private func createSidebarIfNeeded() {
        guard sessionListController == nil else { return }
        
        let controller = SessionListViewController()
        controller.delegate = self
        
        addChild(controller)
        
        controller.view.translatesAutoresizingMaskIntoConstraints = false
        controller.view.wantsLayer = true
        
        // Start hidden (off-screen to the left)
        controller.view.alphaValue = 0
        controller.setInteractive(false)
        
        view.addSubview(controller.view)
        
        // Position sidebar
        controller.view.snp.makeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.bottom.equalToSuperview()
            make.leading.equalToSuperview()
            make.width.equalToSuperview().multipliedBy(0.33)
        }
        
        sessionListController = controller
        
        // Apply cached session data
        if !cachedSessions.isEmpty {
            controller.updateSessions(cachedSessions, activeId: cachedActiveSessionId)
        }
        
        // Force layout to get correct width
        view.layoutSubtreeIfNeeded()
        
        // Set initial transform (hidden off-screen)
        let sidebarWidth = controller.view.bounds.width > 0 ? controller.view.bounds.width : 300
        controller.view.layer?.setAffineTransform(CGAffineTransform(translationX: -sidebarWidth, y: 0))
    }
    
    private func setupTerminalView() {
        // Configure scroll view and command blocks collection
        terminalScrollView.hasVerticalScroller = true
        terminalScrollView.hasHorizontalScroller = false
        terminalScrollView.autohidesScrollers = true
        terminalScrollView.backgroundColor = NSColor.black
        terminalScrollView.borderType = .noBorder
        terminalScrollView.drawsBackground = true

        // Layout configuration: single column, dynamic height
        collectionLayout.minimumLineSpacing = 8
        collectionLayout.minimumInteritemSpacing = 0
        collectionLayout.sectionInset = NSEdgeInsets(top: 8, left: 8, bottom: 8, right: 8)

        collectionView.collectionViewLayout = collectionLayout
        collectionView.isSelectable = false
        collectionView.backgroundColors = [NSColor.black]
        collectionView.delegate = self
        collectionView.dataSource = self
        collectionView.register(CommandBlockItem.self, forItemWithIdentifier: CommandBlockItem.reuseId)

        terminalScrollView.documentView = collectionView

        // Set initial collection frame manually to current scrollView contentSize
        collectionView.frame = NSRect(origin: .zero, size: terminalScrollView.contentSize)

        // print("🔧 CollectionView enabled. TerminalScrollView size: \(terminalScrollView.frame)")

        // Gestures for unified selection
        setupSelectionGestures()
    }
    
    func updateTextViewFrame() {
        // Manual document view (collectionView) sizing with "gravity to bottom"
        let viewport = terminalScrollView.contentSize

        // 1) Document width = viewport width
        var frame = collectionView.frame
        frame.size.width = viewport.width

        // 2) Reset top inset and measure content height
        collectionLayout.sectionInset.top = baseTopInset
        collectionView.collectionViewLayout?.invalidateLayout()
        let contentH0 = collectionView.collectionViewLayout?.collectionViewContentSize.height ?? 0

        // 3) Add extra space at top if content is smaller than viewport
        let extraTop = max(0, viewport.height - contentH0)
        if extraTop > 0 {
            collectionLayout.sectionInset.top = baseTopInset + extraTop
            collectionView.collectionViewLayout?.invalidateLayout()
        }

        // 4) Document height = max(viewport, actual content height)
        let contentH = collectionView.collectionViewLayout?.collectionViewContentSize.height ?? viewport.height
        frame.size.height = max(viewport.height, contentH)

        if collectionView.frame != frame {
            collectionView.frame = frame
        }
    }
    
    private func recreateTextViewWithCorrectSize() {
        // No longer used
    }
    
    private func setupInputView() {
        inputContainerView.wantsLayer = true
        inputContainerView.layer?.backgroundColor = NSColor.black.cgColor
        
        // Disable autoresizing for subviews
        cwdLabel.translatesAutoresizingMaskIntoConstraints = false
        commandTextField.translatesAutoresizingMaskIntoConstraints = false
        sendButton.translatesAutoresizingMaskIntoConstraints = false
        
        // CWD label — purple, like in Warp
        cwdLabel.font = NSFont.monospacedSystemFont(ofSize: 11, weight: .medium)
        cwdLabel.textColor = NSColor(red: 0.6, green: 0.4, blue: 0.9, alpha: 1.0) // Purple
        cwdLabel.backgroundColor = .clear
        cwdLabel.isBordered = false
        cwdLabel.isBezeled = false
        cwdLabel.isEditable = false
        cwdLabel.isSelectable = false
        cwdLabel.lineBreakMode = .byTruncatingHead // Truncate beginning of long paths
        cwdLabel.stringValue = "~"
        inputContainerView.addSubview(cwdLabel)
        
        // Command text field — light text on black background
        commandTextField.placeholderString = "Enter command..."
        commandTextField.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        commandTextField.textColor = NSColor.white
        commandTextField.target = self
        commandTextField.action = #selector(sendCommand)
        commandTextField.tabDelegate = self // Set delegate for Tab key handling
        
        // Remove all visual elements to blend with background
        commandTextField.focusRingType = .none
        commandTextField.isBordered = false
        commandTextField.isBezeled = false
        commandTextField.backgroundColor = NSColor.clear
        commandTextField.drawsBackground = false
        
        // Callback for animated input field auto-resize
        commandTextField.onHeightChanged = { [weak self] newHeight in
            guard let self = self else { return }
            
            NSAnimationContext.runAnimationGroup { context in
                context.duration = 0.15
                context.allowsImplicitAnimation = true
                self.view.layoutSubtreeIfNeeded()
            } completionHandler: {
                self.updateTextViewFrame()
            }
        }
        
        // Thin underline — more visible on black background
        let underlineView = NSView()
        underlineView.translatesAutoresizingMaskIntoConstraints = false
        underlineView.wantsLayer = true
        underlineView.layer?.backgroundColor = NSColor(white: 0.3, alpha: 1.0).cgColor
        inputContainerView.addSubview(underlineView)
        
        // Store reference for layout constraints
        self.inputUnderlineView = underlineView
        
        // Send button — circular button with arrow like in Telegram
        sendButton.wantsLayer = true
        sendButton.isBordered = false
        sendButton.title = ""
        sendButton.bezelStyle = .regularSquare
        
        // SF Symbol arrow
        if let arrowImage = NSImage(systemSymbolName: "arrow.up.circle.fill", accessibilityDescription: "Send") {
            let config = NSImage.SymbolConfiguration(pointSize: 24, weight: .medium)
            sendButton.image = arrowImage.withSymbolConfiguration(config)
            sendButton.imagePosition = .imageOnly
            sendButton.contentTintColor = NSColor(red: 0.0, green: 0.48, blue: 1.0, alpha: 1.0) // Blue like in Telegram
        }
        
        sendButton.target = self
        sendButton.action = #selector(sendCommand)
        sendButton.keyEquivalent = "\r" // Enter key alternative
        
        inputContainerView.addSubview(commandTextField)
        inputContainerView.addSubview(sendButton)
    }
    
    private func setupLayout() {
        // print("🔧 setupLayout: Sizes before constraints:")
        print("   View: \(view.frame)")
        print("   ScrollView: \(terminalScrollView.frame)")
        print("   InputContainer: \(inputContainerView.frame)")
        
        // Force layout update before setting constraints
        view.layoutSubtreeIfNeeded()
        
        // Top toolbar
        topToolbarView.snp.makeConstraints { make in
            make.top.leading.trailing.equalToSuperview()
            make.height.equalTo(36)
        }
        
        hamburgerButton.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(8)
            make.centerY.equalToSuperview()
            make.width.height.equalTo(28)
        }
        
        chatButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-8)
            make.centerY.equalToSuperview()
            make.width.height.equalTo(28)
        }
        
        sessionLabel.snp.makeConstraints { make in
            make.center.equalToSuperview()
        }
        
        // Terminal view - fills space from toolbar to input
        terminalScrollView.snp.makeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.leading.trailing.equalToSuperview()
            make.height.greaterThanOrEqualTo(200)
        }
        
        // Store constraint for dynamic changes in raw mode
        updateTerminalBottomConstraint(isRawMode: false)
        
        // print("🔧 Terminal constraints set with minimum height 200")
        
        // Input container - dynamic height
        inputContainerView.snp.makeConstraints { make in
            make.leading.trailing.equalToSuperview()
            make.bottom.equalToSuperview()
        }
        
        // CWD label at top
        cwdLabel.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.trailing.equalToSuperview().offset(-12)
            make.top.equalToSuperview().offset(6)
            make.height.equalTo(16)
        }
        
        // Text field - dynamic height (min 24, grows with content)
        commandTextField.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.top.equalTo(cwdLabel.snp.bottom).offset(4)
            make.trailing.equalTo(sendButton.snp.leading).offset(-8)
            make.height.greaterThanOrEqualTo(24)
            make.bottom.equalToSuperview().offset(-12) // Determines container height
        }
        
        sendButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-12)
            make.top.equalTo(commandTextField.snp.top) // Align with text field top
            make.width.height.equalTo(28)
        }
        
        // Underline view constraints
        inputUnderlineView.snp.makeConstraints { make in
            make.leading.equalTo(commandTextField.snp.leading)
            make.trailing.equalTo(commandTextField.snp.trailing)
            make.bottom.equalTo(commandTextField.snp.bottom).offset(2)
            make.height.equalTo(1)
        }
        
        // print("🔧 setupLayout completed: all constraints set")
    }
    
    /// Updates bottom constraint of command list depending on mode
    private func updateTerminalBottomConstraint(isRawMode: Bool) {
        terminalScrollView.snp.remakeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.leading.trailing.equalToSuperview()
            make.height.greaterThanOrEqualTo(200)
            
            if isRawMode {
                // In raw mode, list stretches to the bottom
                make.bottom.equalToSuperview()
            } else {
                // In normal mode, list ends before input field
                make.bottom.equalTo(inputContainerView.snp.top)
            }
        }
        
        // Update selection highlight after layout change
        DispatchQueue.main.async { [weak self] in
            self?.updateSelectionHighlight()
        }
    }
    
    private func setupActions() {
        // Actions already set in setup methods
    }
    
    // MARK: - Sidebar Animation
    @objc func toggleSidebar() {
        // Create sidebar on first use (lazy initialization)
        createSidebarIfNeeded()
        
        guard let sidebarView = sessionListController?.view else { return }
        
        // Toggle state
        isSidebarVisible = !isSidebarVisible
        
        let sidebarWidth = sidebarView.frame.width > 0 ? sidebarView.frame.width : 250
        let targetX: CGFloat = isSidebarVisible ? 0 : -sidebarWidth
        let targetAlpha: CGFloat = isSidebarVisible ? 1.0 : 0.0
        
        // Enable interaction before showing
        if isSidebarVisible {
            sessionListController?.setInteractive(true)
        }
        
        NSAnimationContext.runAnimationGroup({ context in
            context.duration = 0.15
            context.allowsImplicitAnimation = true
            sidebarView.layer?.setAffineTransform(CGAffineTransform(translationX: targetX, y: 0))
            sidebarView.alphaValue = targetAlpha
        }, completionHandler: { [weak self] in
            guard let self = self else { return }
            // Disable interaction after hiding
            if !self.isSidebarVisible {
                self.sessionListController?.setInteractive(false)
                self.view.window?.makeFirstResponder(self.commandTextField)
            }
        })
    }
    
    /// Hides sidebar without animation (for cleanup)
    func hideSidebar() {
        guard let sidebarView = sessionListController?.view else {
            isSidebarVisible = false
            return
        }
        
        isSidebarVisible = false
        let sidebarWidth = sidebarView.bounds.width > 0 ? sidebarView.bounds.width : 300
        sidebarView.layer?.setAffineTransform(CGAffineTransform(translationX: -sidebarWidth, y: 0))
        sidebarView.alphaValue = 0
        sessionListController?.setInteractive(false)
    }
    
    /// Updates sidebar position when view size changes
    private func updateSidebarTransform() {
        guard !isSidebarVisible, let sidebarView = sessionListController?.view else { return }
        let sidebarWidth = sidebarView.bounds.width > 0 ? sidebarView.bounds.width : 300
        sidebarView.layer?.setAffineTransform(CGAffineTransform(translationX: -sidebarWidth, y: 0))
    }
    
    // MARK: - Chat Sidebar
    
    /// Sets up chat sidebar view on first toggle (controller already created in viewDidLoad)
    private var isChatSidebarViewSetup = false
    
    private func setupChatSidebarViewIfNeeded() {
        guard !isChatSidebarViewSetup else { return }
        isChatSidebarViewSetup = true
        
        chatSidebarController.sessionId = cachedActiveSessionId
        
        chatSidebarController.view.translatesAutoresizingMaskIntoConstraints = false
        chatSidebarController.view.wantsLayer = true
        
        // Start hidden (off-screen to the right)
        chatSidebarController.view.alphaValue = 0
        chatSidebarController.setInteractive(false)
        
        view.addSubview(chatSidebarController.view)
        
        // Position chat sidebar on the right
        chatSidebarController.view.snp.makeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.bottom.equalToSuperview()
            make.trailing.equalToSuperview()
            make.width.equalToSuperview().multipliedBy(0.35)
        }
        
        // Force layout to get correct width
        view.layoutSubtreeIfNeeded()
        
        // Set initial transform (hidden off-screen to the right)
        let sidebarWidth = chatSidebarController.view.bounds.width > 0 ? chatSidebarController.view.bounds.width : 300
        chatSidebarController.view.layer?.setAffineTransform(CGAffineTransform(translationX: sidebarWidth, y: 0))
    }
    
    @objc func toggleChatSidebar() {
        // Setup view on first use (controller already exists)
        setupChatSidebarViewIfNeeded()
        
        let chatView = chatSidebarController.view
        
        // Toggle state
        isChatSidebarVisible = !isChatSidebarVisible
        
        let sidebarWidth = chatView.frame.width > 0 ? chatView.frame.width : 300
        let targetX: CGFloat = isChatSidebarVisible ? 0 : sidebarWidth
        let targetAlpha: CGFloat = isChatSidebarVisible ? 1.0 : 0.0
        
        // Enable interaction before showing
        if isChatSidebarVisible {
            chatSidebarController.setInteractive(true)
        }
        
        NSAnimationContext.runAnimationGroup({ context in
            context.duration = 0.15
            context.allowsImplicitAnimation = true
            chatView.layer?.setAffineTransform(CGAffineTransform(translationX: targetX, y: 0))
            chatView.alphaValue = targetAlpha
        }, completionHandler: { [weak self] in
            guard let self = self else { return }
            if self.isChatSidebarVisible {
                // Focus input field after showing
                self.chatSidebarController.focusInputField()
            } else {
                // Disable interaction and return focus to command input
                self.chatSidebarController.setInteractive(false)
                self.view.window?.makeFirstResponder(self.commandTextField)
            }
        })
    }
    
    /// Updates chat sidebar position when view size changes
    private func updateChatSidebarTransform() {
        guard !isChatSidebarVisible else { return }
        let chatView = chatSidebarController.view
        let sidebarWidth = chatView.bounds.width > 0 ? chatView.bounds.width : 300
        chatView.layer?.setAffineTransform(CGAffineTransform(translationX: sidebarWidth, y: 0))
    }
    
    // MARK: - AI Chat Public API
    
    /// Start streaming assistant response
    func aiStartResponse() {
        chatSidebarController.startAssistantMessage()
    }
    
    /// Append chunk to current streaming response
    func aiAppendChunk(_ text: String) {
        chatSidebarController.appendChunk(text)
    }
    
    /// Finish streaming response
    func aiFinishResponse() {
        chatSidebarController.finishAssistantMessage()
    }
    
    /// Show AI error
    func aiShowError(_ error: String) {
        chatSidebarController.showError(error)
    }
    
    /// Update LLM providers
    func updateLLMProviders(_ providers: [LLMProvider]) {
        chatSidebarController.updateProviders(providers)
    }
    
    /// Access to chat sidebar for delegate setting
    var chatSidebarViewController: ChatSidebarViewController? {
        return chatSidebarController
    }
    
    // MARK: - Public Methods
    func configure(serverAddress: String) {
        self.serverAddress = serverAddress
        // Set window title
        view.window?.title = "TermiHUI — \(serverAddress)"
    }
    
    /// Clears terminal state (called on disconnect)
    func clearState() {
        commandBlocks.removeAll()
        currentBlockIndex = nil
        globalDocument = GlobalDocument()
        selectionRange = nil
        selectionAnchor = nil
        currentCwd = ""
        serverHome = ""
        cwdLabel.stringValue = "~"
        sessionLabel.stringValue = ""
        collectionView.reloadData()
        
        // Reset raw input mode
        isCommandRunning = false
        inputContainerView.isHidden = false
        inputContainerView.alphaValue = 1
        
        // Reset interactive mode
        if isInteractiveMode {
            exitInteractiveMode()
        }
        terminalScrollView.isHidden = false
        terminalScrollView.alphaValue = 1
    }
    
    /// Append raw output (backward compatibility - creates single unstyled segment)
    func appendOutput(_ output: String) {
        let segment = StyledSegment(text: output, style: SegmentStyle())
        appendStyledOutput([segment])
    }
    
    /// Append pre-parsed styled segments from C++ core
    func appendStyledOutput(_ segments: [StyledSegment]) {
        guard !segments.isEmpty else { return }
        
        // Accumulate output in current block (if there's an unfinished one)
        if let idx = currentBlockIndex {
            commandBlocks[idx].outputSegments.append(contentsOf: segments)
            reloadBlock(at: idx)
            rebuildGlobalDocument(startingAt: idx)
        } else {
            // If no block exists (e.g., output outside command) — create standalone block
            let block = CommandBlock(commandId: nil, command: nil, outputSegments: segments, isFinished: false, exitCode: nil, cwdStart: nil, cwdEnd: nil)
            commandBlocks.append(block)
            let newIndex = commandBlocks.count - 1
            insertBlock(at: newIndex)
            currentBlockIndex = newIndex
            rebuildGlobalDocument(startingAt: newIndex)
        }
    }
    
    func showConnectionStatus(_ status: String) {
        // Status is now in window title
        if !serverAddress.isEmpty {
            view.window?.title = "TermiHUI — \(serverAddress) (\(status))"
        }
    }
    
    /// Updates server home directory (for path shortening)
    func updateServerHome(_ home: String) {
        serverHome = home
        // Update CWD display with new home
        if !currentCwd.isEmpty {
            updateCurrentCwd(currentCwd)
        }
    }
    
    /// Updates current working directory display
    func updateCurrentCwd(_ cwd: String) {
        currentCwd = cwd
        // Shorten path only if server provided home directory
        let displayCwd: String
        if !serverHome.isEmpty && cwd.hasPrefix(serverHome) {
            displayCwd = "~" + String(cwd.dropFirst(serverHome.count))
        } else {
            displayCwd = cwd
        }
        cwdLabel.stringValue = displayCwd
        // print("📂 CWD updated: \(displayCwd)")
    }
    
    /// Updates current session name in toolbar
    func updateSessionName(_ sessionId: UInt64?) {
        if let id = sessionId {
            sessionLabel.stringValue = "#\(id)"
        } else {
            sessionLabel.stringValue = ""
        }
    }
    
    // MARK: - Actions
    @objc private func sendCommand() {
        let command = commandTextField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        
        guard !command.isEmpty else { return }
        
        // Clear input field
        commandTextField.stringValue = ""
        commandTextField.updateHeightIfNeeded() // Reset field height
        
        // DON'T add command echo - PTY already provides full output
        // appendOutput("$ \(command)\n")  // Remove duplication
        
        // Send command via delegate
        delegate?.terminalViewController(self, didSendCommand: command)
    }
    
    /// Called from Client -> Disconnect menu
    func requestDisconnect() {
        delegate?.terminalViewControllerDidRequestDisconnect(self)
    }
    
    // MARK: - Raw Input Mode
    
    /// Counter to track animation state and prevent race conditions
    private var rawModeAnimationCounter: Int = 0
    
    /// Enters raw input mode when command starts executing
    func enterRawInputMode() {
        isCommandRunning = true
        rawModeAnimationCounter += 1
        let currentCounter = rawModeAnimationCounter
        
        // Stretch command list to bottom and hide input field
        NSAnimationContext.runAnimationGroup { context in
            context.duration = 0.15
            inputContainerView.animator().alphaValue = 0
        } completionHandler: { [weak self] in
            guard let self = self else { return }
            // Only proceed if we're still in the same animation cycle
            if self.rawModeAnimationCounter == currentCounter && self.isCommandRunning {
                self.inputContainerView.isHidden = true
                self.updateTerminalBottomConstraint(isRawMode: true)
                self.view.layoutSubtreeIfNeeded()
            }
        }
        
        // Make terminal view first responder to capture keyboard
        view.window?.makeFirstResponder(self)
        
        print("🎹 Entered raw input mode")
    }
    
    /// Exits raw input mode when command finishes
    func exitRawInputMode() {
        isCommandRunning = false
        rawModeAnimationCounter += 1 // Invalidate any pending hide animations
        
        // Restore layout: command list before input field
        updateTerminalBottomConstraint(isRawMode: false)
        
        // Show input container immediately (no animation to avoid race)
        inputContainerView.isHidden = false
        inputContainerView.alphaValue = 1
        
        // Update layout
        view.layoutSubtreeIfNeeded()
        
        // Return focus to command text field
        view.window?.makeFirstResponder(commandTextField)
        
        print("🎹 Exited raw input mode")
    }
    
    /// Sends raw input to PTY via WebSocket
    func sendRawInput(_ text: String) {
        // Local echo for printable characters and newlines - ONLY in non-interactive mode
        // In interactive mode, the app handles its own display via screen_diff
        if !isInteractiveMode {
            // Note: Real terminals handle echo on PTY side, but we disabled it
            // to avoid command duplication. So we do client-side echo in raw mode.
            if text == "\n" || text == "\r" || text == "\r\n" {
                appendOutput("\n")
            } else if text.allSatisfy({ $0.isPrintable }) {
                appendOutput(text)
            }
            // Don't echo control characters (Ctrl+C, escape sequences, etc.)
        }
        
        clientCore?.send(["type": "sendInput", "text": text])
    }
    
    // MARK: - Interactive Mode (Full Terminal Emulation)
    
    /// Enter interactive mode (alternate screen buffer)
    func enterInteractiveMode(rows: Int, columns: Int) {
        guard !isInteractiveMode else { return }
        isInteractiveMode = true
        interactiveScreenRows = rows
        interactiveScreenColumns = columns
        interactiveScreenLines = Array(repeating: [], count: rows)
        
        print("🖥️ Entering interactive mode: \(columns)x\(rows)")
        
        // Create interactive terminal view if needed
        setupInteractiveView()
        
        // Show interactive view immediately (but transparent)
        interactiveScrollView?.isHidden = false
        interactiveScrollView?.alphaValue = 0
        
        // Hide block-based UI with animation
        NSAnimationContext.runAnimationGroup { context in
            context.duration = 0.15
            terminalScrollView.animator().alphaValue = 0
            inputContainerView.animator().alphaValue = 0
            interactiveScrollView?.animator().alphaValue = 1
        } completionHandler: { [weak self] in
            guard let self = self, self.isInteractiveMode else { return }
            self.terminalScrollView.isHidden = true
            self.inputContainerView.isHidden = true
            
            // Make TerminalViewController first responder AFTER animation completes
            // (otherwise hiding inputContainerView resets the first responder)
            _ = self.view.window?.makeFirstResponder(self)
        }
    }
    
    /// Exit interactive mode (return to block-based UI)
    func exitInteractiveMode() {
        guard isInteractiveMode else { return }
        isInteractiveMode = false
        
        print("🖥️ Exiting interactive mode")
        
        // Hide interactive view
        interactiveScrollView?.isHidden = true
        
        // Show block-based UI
        terminalScrollView.isHidden = false
        inputContainerView.isHidden = false
        terminalScrollView.alphaValue = 1
        inputContainerView.alphaValue = 1
        
        // Clear interactive state
        interactiveScreenLines = []
        
        // Return focus to command text field
        view.window?.makeFirstResponder(commandTextField)
    }
    
    /// Setup interactive terminal view
    private func setupInteractiveView() {
        if interactiveScrollView != nil { return }
        
        // Create scroll view
        let scrollView = NSScrollView()
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = false
        scrollView.autohidesScrollers = true
        scrollView.backgroundColor = .black
        scrollView.drawsBackground = true
        scrollView.borderType = .noBorder
        scrollView.isHidden = true
        
        // Create text view
        let textView = NSTextView()
        textView.isEditable = false
        textView.isSelectable = true
        textView.backgroundColor = .black
        textView.textContainerInset = NSSize(width: 0, height: 0)
        textView.font = terminalFont
        textView.textColor = .lightGray
        textView.isRichText = true
        textView.allowsUndo = false
        
        // Set text container for proper wrapping
        textView.textContainer?.widthTracksTextView = true
        textView.textContainer?.heightTracksTextView = false
        textView.textContainer?.lineFragmentPadding = 0
        textView.isVerticallyResizable = true
        textView.isHorizontallyResizable = false
        
        scrollView.documentView = textView
        
        view.addSubview(scrollView)
        
        // Layout: fill entire area below toolbar
        scrollView.snp.makeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.leading.trailing.bottom.equalToSuperview()
        }
        
        interactiveScrollView = scrollView
        interactiveTextView = textView
    }
    
    /// Handle full screen snapshot from server
    func handleScreenSnapshot(lines: [[StyledSegment]], cursorRow: Int, cursorColumn: Int) {
        // Auto-enter interactive mode if we receive snapshot but not in interactive mode
        // This happens when client reconnects while server is in interactive mode
        if !isInteractiveMode {
            print("🖥️ Auto-entering interactive mode (received snapshot)")
            enterInteractiveMode(rows: lines.count, columns: 80)
        }
        
        interactiveScreenLines = lines
        interactiveCursorRow = cursorRow
        interactiveCursorColumn = cursorColumn
        
        renderInteractiveScreen()
    }
    
    /// Handle screen diff (partial update)
    func handleScreenDiff(updates: [ScreenRowUpdate], cursorRow: Int, cursorColumn: Int) {
        guard isInteractiveMode else { return }
        
        // Apply updates
        for update in updates {
            if update.row < interactiveScreenLines.count {
                interactiveScreenLines[update.row] = update.segments
            }
        }
        
        interactiveCursorRow = cursorRow
        interactiveCursorColumn = cursorColumn
        
        renderInteractiveScreen()
    }
    
    /// Render interactive screen to text view
    private func renderInteractiveScreen() {
        guard let textView = interactiveTextView else { return }
        
        let result = NSMutableAttributedString()
        
        // Create paragraph style with fixed line height
        let paragraphStyle = NSMutableParagraphStyle()
        let lineHeight = ceil(terminalFont.ascender - terminalFont.descender + terminalFont.leading)
        paragraphStyle.minimumLineHeight = lineHeight
        paragraphStyle.maximumLineHeight = lineHeight
        
        let defaultAttrs: [NSAttributedString.Key: Any] = [
            .font: terminalFont,
            .foregroundColor: NSColor.lightGray,
            .paragraphStyle: paragraphStyle
        ]
        
        for (rowIndex, lineSegments) in interactiveScreenLines.enumerated() {
            if rowIndex > 0 {
                result.append(NSAttributedString(string: "\n", attributes: defaultAttrs))
            }
            
            let lineStartIndex = result.length
            
            if lineSegments.isEmpty {
                // Empty line - use space to maintain line height and allow cursor
                result.append(NSAttributedString(string: " ", attributes: defaultAttrs))
            } else {
                // Add paragraph style to segments
                let lineStr = lineSegments.toAttributedString()
                let mutableLine = NSMutableAttributedString(attributedString: lineStr)
                mutableLine.addAttribute(.paragraphStyle, value: paragraphStyle, range: NSRange(location: 0, length: mutableLine.length))
                result.append(mutableLine)
            }
            
            // Render cursor on this row
            if rowIndex == interactiveCursorRow {
                let cursorPosInLine = interactiveCursorColumn
                let lineLength = result.length - lineStartIndex
                
                if cursorPosInLine < lineLength {
                    // Cursor is within existing text - invert the character
                    let cursorRange = NSRange(location: lineStartIndex + cursorPosInLine, length: 1)
                    result.addAttribute(.backgroundColor, value: NSColor.lightGray, range: cursorRange)
                    result.addAttribute(.foregroundColor, value: NSColor.black, range: cursorRange)
                } else {
                    // Cursor is beyond text - add cursor block
                    let cursorAttrs: [NSAttributedString.Key: Any] = [
                        .font: terminalFont,
                        .foregroundColor: NSColor.black,
                        .backgroundColor: NSColor.lightGray,
                        .paragraphStyle: paragraphStyle
                    ]
                    // Pad with spaces if needed
                    let spacesNeeded = cursorPosInLine - lineLength
                    if spacesNeeded > 0 {
                        result.append(NSAttributedString(string: String(repeating: " ", count: spacesNeeded), attributes: defaultAttrs))
                    }
                    result.append(NSAttributedString(string: " ", attributes: cursorAttrs))
                }
            }
        }
        
        textView.textStorage?.setAttributedString(result)
        
        // Don't auto-scroll - let user control scroll position
    }
}
