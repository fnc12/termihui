import Cocoa
import SnapKit

/// Main terminal screen
class TerminalViewController: NSViewController, NSGestureRecognizerDelegate {
    
    // MARK: - UI Components
    private let topToolbarView = NSView()
    private let hamburgerButton = NSButton()
    private let sessionLabel = NSTextField(labelWithString: "")
    
    let terminalScrollView = NSScrollView()
    var collectionView = NSCollectionView()
    let collectionLayout = NSCollectionViewFlowLayout()
    
    private let inputContainerView = NSView()
    private let cwdLabel = NSTextField(labelWithString: "")
    let commandTextField = TabHandlingTextField()
    private let sendButton = NSButton(title: "Отправить", target: nil, action: nil)
    private var inputUnderlineView: NSView!
    
    // Session sidebar
    var sessionListController: SessionListViewController?
    private var isSidebarVisible = false
    
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
        let id: UUID
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
        
        print("🔧 viewDidAppear: Parent view size: \(view.frame)")
        print("🔧 viewDidAppear: ScrollView size after layout: \(terminalScrollView.frame)")
        
        // Hide sidebar initially (after layout so we know the width)
        hideSidebar()
        
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
        
        // Update sidebar transform when view size changes
        updateSidebarTransform()
        
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
            
            print("📐 Terminal size changed: \(cols)x\(rows)")
            clientCore?.send(["type": "resize", "cols": cols, "rows": rows])
        }
    }
    
    /// Force send current terminal size (e.g., on initial connection)
    func sendInitialTerminalSize() {
        let (cols, rows) = calculateTerminalSize()
        lastTerminalCols = cols
        lastTerminalRows = rows
        
        print("📐 Initial terminal size: \(cols)x\(rows)")
        clientCore?.send(["type": "resize", "cols": cols, "rows": rows])
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
        setupToolbarView()
        setupTerminalView()
        setupInputView()
        
        // Add main views
        view.addSubview(terminalScrollView)
        view.addSubview(inputContainerView)
        view.addSubview(topToolbarView) // Toolbar on top (last = front)
        
        // Sidebar must be added after topToolbarView (for constraint)
        setupSessionSidebar()
    }
    
    private func setupToolbarView() {
        topToolbarView.wantsLayer = true
        topToolbarView.layer?.backgroundColor = NSColor(white: 0.15, alpha: 1.0).cgColor
        
        // Hamburger button (menu icon)
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
    
    private func setupSessionSidebar() {
        let controller = SessionListViewController()
        controller.delegate = self
        
        // Add as child view controller
        addChild(controller)
        view.addSubview(controller.view)
        controller.view.wantsLayer = true
        
        // Position sidebar
        controller.view.snp.makeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.bottom.equalToSuperview()
            make.leading.equalToSuperview()
            make.width.equalToSuperview().multipliedBy(0.33)
        }
        
        sessionListController = controller
        
        // Initially hidden - apply transform after layout
        DispatchQueue.main.async { [weak self] in
            guard let sidebarView = self?.sessionListController?.view else { return }
            let sidebarWidth = sidebarView.bounds.width > 0 ? sidebarView.bounds.width : 300
            sidebarView.layer?.setAffineTransform(CGAffineTransform(translationX: -sidebarWidth, y: 0))
        }
    }
    
    private func setupTerminalView() {
        // Настраиваем scroll view и коллекцию блоков
        terminalScrollView.hasVerticalScroller = true
        terminalScrollView.hasHorizontalScroller = false
        terminalScrollView.autohidesScrollers = true
        terminalScrollView.backgroundColor = NSColor.black
        terminalScrollView.borderType = .noBorder
        terminalScrollView.drawsBackground = true

        // Конфигурация layout: одна колонка, динамическая высота
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

        print("🔧 CollectionView enabled. TerminalScrollView size: \(terminalScrollView.frame)")

        // Gestures for unified selection
        setupSelectionGestures()
    }
    
    func updateTextViewFrame() {
        // Ручное управление размерами documentView (collectionView) и «гравитация вниз»
        let viewport = terminalScrollView.contentSize

        // 1) ширина документа = ширине viewport
        var frame = collectionView.frame
        frame.size.width = viewport.width

        // 2) сбросить верхний inset и измерить высоту контента
        collectionLayout.sectionInset.top = baseTopInset
        collectionView.collectionViewLayout?.invalidateLayout()
        let contentH0 = collectionView.collectionViewLayout?.collectionViewContentSize.height ?? 0

        // 3) добавляем пустое пространство сверху, если контента мало
        let extraTop = max(0, viewport.height - contentH0)
        if extraTop > 0 {
            collectionLayout.sectionInset.top = baseTopInset + extraTop
            collectionView.collectionViewLayout?.invalidateLayout()
        }

        // 4) высота документа = max(viewport, фактическая высота контента)
        let contentH = collectionView.collectionViewLayout?.collectionViewContentSize.height ?? viewport.height
        frame.size.height = max(viewport.height, contentH)

        if collectionView.frame != frame {
            collectionView.frame = frame
        }
    }
    
    private func recreateTextViewWithCorrectSize() {
        // Больше не используется
    }
    
    private func setupInputView() {
        inputContainerView.wantsLayer = true
        inputContainerView.layer?.backgroundColor = NSColor.black.cgColor
        
        // CWD label — фиолетовый, как в Warp
        cwdLabel.font = NSFont.monospacedSystemFont(ofSize: 11, weight: .medium)
        cwdLabel.textColor = NSColor(red: 0.6, green: 0.4, blue: 0.9, alpha: 1.0) // Фиолетовый
        cwdLabel.backgroundColor = .clear
        cwdLabel.isBordered = false
        cwdLabel.isBezeled = false
        cwdLabel.isEditable = false
        cwdLabel.isSelectable = false
        cwdLabel.lineBreakMode = .byTruncatingHead // Обрезаем начало пути, если длинный
        cwdLabel.stringValue = "~"
        inputContainerView.addSubview(cwdLabel)
        
        // Command text field — светлый текст на чёрном фоне
        commandTextField.placeholderString = "Введите команду..."
        commandTextField.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        commandTextField.textColor = NSColor.white
        commandTextField.target = self
        commandTextField.action = #selector(sendCommand)
        commandTextField.tabDelegate = self // Устанавливаем делегат для Tab-обработки
        
        // Убираем все визуальные элементы поля для слияния с фоном
        commandTextField.focusRingType = .none
        commandTextField.isBordered = false
        commandTextField.isBezeled = false
        commandTextField.backgroundColor = NSColor.clear
        commandTextField.drawsBackground = false
        
        // Callback для авторазмера поля ввода с анимацией
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
        
        // Тонкая линия снизу — более контрастная на чёрном фоне
        let underlineView = NSView()
        underlineView.wantsLayer = true
        underlineView.layer?.backgroundColor = NSColor(white: 0.3, alpha: 1.0).cgColor
        inputContainerView.addSubview(underlineView)
        
        // Сохраняем ссылку для layout constraints
        self.inputUnderlineView = underlineView
        
        // Send button — круглая кнопка со стрелкой как в Telegram
        sendButton.wantsLayer = true
        sendButton.isBordered = false
        sendButton.title = ""
        sendButton.bezelStyle = .regularSquare
        
        // SF Symbol стрелка
        if let arrowImage = NSImage(systemSymbolName: "arrow.up.circle.fill", accessibilityDescription: "Send") {
            let config = NSImage.SymbolConfiguration(pointSize: 24, weight: .medium)
            sendButton.image = arrowImage.withSymbolConfiguration(config)
            sendButton.imagePosition = .imageOnly
            sendButton.contentTintColor = NSColor(red: 0.0, green: 0.48, blue: 1.0, alpha: 1.0) // Синий как в Telegram
        }
        
        sendButton.target = self
        sendButton.action = #selector(sendCommand)
        sendButton.keyEquivalent = "\r" // Enter key alternative
        
        inputContainerView.addSubview(commandTextField)
        inputContainerView.addSubview(sendButton)
    }
    
    private func setupLayout() {
        print("🔧 setupLayout: Sizes before constraints:")
        print("   View: \(view.frame)")
        print("   ScrollView: \(terminalScrollView.frame)")
        print("   InputContainer: \(inputContainerView.frame)")
        
        // Принудительно обновляем layout перед установкой constraints
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
        
        sessionLabel.snp.makeConstraints { make in
            make.center.equalToSuperview()
        }
        
        // Terminal view - занимает всё пространство от toolbar до input
        terminalScrollView.snp.makeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.leading.trailing.equalToSuperview()
            make.height.greaterThanOrEqualTo(200)
        }
        
        // Сохраняем constraint для динамического изменения при raw mode
        updateTerminalBottomConstraint(isRawMode: false)
        
        print("🔧 Terminal constraints set with minimum height 200")
        
        // Input container - динамическая высота
        inputContainerView.snp.makeConstraints { make in
            make.leading.trailing.equalToSuperview()
            make.bottom.equalToSuperview()
        }
        
        // CWD label сверху
        cwdLabel.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.trailing.equalToSuperview().offset(-12)
            make.top.equalToSuperview().offset(6)
            make.height.equalTo(16)
        }
        
        // Text field - динамическая высота (min 24, растёт по контенту)
        commandTextField.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.top.equalTo(cwdLabel.snp.bottom).offset(4)
            make.trailing.equalTo(sendButton.snp.leading).offset(-8)
            make.height.greaterThanOrEqualTo(24)
            make.bottom.equalToSuperview().offset(-12) // Определяет высоту контейнера
        }
        
        sendButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-12)
            make.top.equalTo(commandTextField.snp.top) // Выравниваем по верху поля
            make.width.height.equalTo(28)
        }
        
        // Underline view constraints
        inputUnderlineView.snp.makeConstraints { make in
            make.leading.equalTo(commandTextField.snp.leading)
            make.trailing.equalTo(commandTextField.snp.trailing)
            make.bottom.equalTo(commandTextField.snp.bottom).offset(2)
            make.height.equalTo(1)
        }
        
        print("🔧 setupLayout completed: all constraints set")
    }
    
    /// Обновляет нижний constraint списка команд в зависимости от режима
    private func updateTerminalBottomConstraint(isRawMode: Bool) {
        terminalScrollView.snp.remakeConstraints { make in
            make.top.equalTo(topToolbarView.snp.bottom)
            make.leading.trailing.equalToSuperview()
            make.height.greaterThanOrEqualTo(200)
            
            if isRawMode {
                // В raw mode список растягивается до самого низа
                make.bottom.equalToSuperview()
            } else {
                // В обычном режиме список заканчивается перед полем ввода
                make.bottom.equalTo(inputContainerView.snp.top)
            }
        }
        
        // После изменения layout обновляем выделение
        DispatchQueue.main.async { [weak self] in
            self?.updateSelectionHighlight()
        }
    }
    
    private func setupActions() {
        // Actions already set in setup methods
    }
    
    // MARK: - Sidebar Animation
    @objc func toggleSidebar() {
        guard let sidebarView = sessionListController?.view else { return }
        
        // Toggle state
        isSidebarVisible = !isSidebarVisible
        
        let sidebarWidth = sidebarView.frame.width > 0 ? sidebarView.frame.width : 250
        let targetX: CGFloat = isSidebarVisible ? 0 : -sidebarWidth
        
        NSAnimationContext.runAnimationGroup({ context in
            context.duration = 0.15
            context.allowsImplicitAnimation = true
            sidebarView.layer?.setAffineTransform(CGAffineTransform(translationX: targetX, y: 0))
        }, completionHandler: { [weak self] in
            // Restore focus to command field after closing
            if !(self?.isSidebarVisible ?? false) {
                self?.view.window?.makeFirstResponder(self?.commandTextField)
            }
        })
    }
    
    /// Hides sidebar without animation (for cleanup)
    func hideSidebar() {
        guard let sidebarView = sessionListController?.view else { return }
        
        isSidebarVisible = false
        let sidebarWidth = sidebarView.bounds.width
        sidebarView.layer?.setAffineTransform(CGAffineTransform(translationX: -sidebarWidth, y: 0))
    }
    
    /// Updates sidebar position when view size changes
    private func updateSidebarTransform() {
        guard !isSidebarVisible, let sidebarView = sessionListController?.view else { return }
        let sidebarWidth = sidebarView.bounds.width
        sidebarView.layer?.setAffineTransform(CGAffineTransform(translationX: -sidebarWidth, y: 0))
    }
    
    // MARK: - Public Methods
    func configure(serverAddress: String) {
        self.serverAddress = serverAddress
        // Устанавливаем заголовок окна
        view.window?.title = "TermiHUI — \(serverAddress)"
    }
    
    /// Очищает состояние терминала (вызывается при отключении)
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
    }
    
    /// Append raw output (backward compatibility - creates single unstyled segment)
    func appendOutput(_ output: String) {
        let segment = StyledSegment(text: output, style: SegmentStyle())
        appendStyledOutput([segment])
    }
    
    /// Append pre-parsed styled segments from C++ core
    func appendStyledOutput(_ segments: [StyledSegment]) {
        guard !segments.isEmpty else { return }
        
        // Копим вывод в текущем блоке (если есть незавершённый)
        if let idx = currentBlockIndex {
            commandBlocks[idx].outputSegments.append(contentsOf: segments)
            reloadBlock(at: idx)
            rebuildGlobalDocument(startingAt: idx)
        } else {
            // Если блока нет (например, вывод вне команды) — создаём самостоятельный блок
            let block = CommandBlock(id: UUID(), command: nil, outputSegments: segments, isFinished: false, exitCode: nil, cwdStart: nil, cwdEnd: nil)
            commandBlocks.append(block)
            let newIndex = commandBlocks.count - 1
            insertBlock(at: newIndex)
            currentBlockIndex = newIndex
            rebuildGlobalDocument(startingAt: newIndex)
        }
    }
    
    func showConnectionStatus(_ status: String) {
        // Статус теперь в заголовке окна
        if !serverAddress.isEmpty {
            view.window?.title = "TermiHUI — \(serverAddress) (\(status))"
        }
    }
    
    /// Обновляет home directory сервера (для сокращения путей)
    func updateServerHome(_ home: String) {
        serverHome = home
        // Обновим отображение CWD с новым home
        if !currentCwd.isEmpty {
            updateCurrentCwd(currentCwd)
        }
    }
    
    /// Обновляет отображение текущей рабочей директории
    func updateCurrentCwd(_ cwd: String) {
        currentCwd = cwd
        // Сокращаем путь только если сервер прислал home
        let displayCwd: String
        if !serverHome.isEmpty && cwd.hasPrefix(serverHome) {
            displayCwd = "~" + String(cwd.dropFirst(serverHome.count))
        } else {
            displayCwd = cwd
        }
        cwdLabel.stringValue = displayCwd
        print("📂 CWD updated: \(displayCwd)")
    }
    
    /// Обновляет название текущей сессии в toolbar
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
        
        // Очищаем поле ввода
        commandTextField.stringValue = ""
        commandTextField.updateHeightIfNeeded() // Сбрасываем высоту поля
        
        // НЕ добавляем эхо команды - PTY уже предоставляет полный вывод
        // appendOutput("$ \(command)\n")  // Убираем дублирование
        
        // Отправляем команду через delegate
        delegate?.terminalViewController(self, didSendCommand: command)
    }
    
    /// Вызывается из меню Client -> Disconnect
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
        
        // Растягиваем список команд до низа и прячем поле ввода
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
        
        // Возвращаем layout: список команд до поля ввода
        updateTerminalBottomConstraint(isRawMode: false)
        
        // Show input container immediately (no animation to avoid race)
        inputContainerView.isHidden = false
        inputContainerView.alphaValue = 1
        
        // Обновляем layout
        view.layoutSubtreeIfNeeded()
        
        // Return focus to command text field
        view.window?.makeFirstResponder(commandTextField)
        
        print("🎹 Exited raw input mode")
    }
    
    /// Sends raw input to PTY via WebSocket
    func sendRawInput(_ text: String) {
        // Local echo for printable characters and newlines
        // Note: Real terminals handle echo on PTY side, but we disabled it
        // to avoid command duplication. So we do client-side echo in raw mode.
        if text == "\n" || text == "\r" || text == "\r\n" {
            appendOutput("\n")
        } else if text.allSatisfy({ $0.isPrintable }) {
            appendOutput(text)
        }
        // Don't echo control characters (Ctrl+C, escape sequences, etc.)
        
        clientCore?.send(["type": "sendInput", "text": text])
    }
}
