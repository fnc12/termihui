import Cocoa
import SnapKit

/// Main terminal screen
class TerminalViewController: NSViewController, NSGestureRecognizerDelegate {
    
    // MARK: - UI Components
    private let terminalScrollView = NSScrollView()
    private var collectionView = NSCollectionView()
    private let collectionLayout = NSCollectionViewFlowLayout()
    
    private let inputContainerView = NSView()
    private let cwdLabel = NSTextField(labelWithString: "")
    private let commandTextField = TabHandlingTextField()
    private let sendButton = NSButton(title: "Отправить", target: nil, action: nil)
    private var inputUnderlineView: NSView!
    
    // Current working directory and server home
    private var currentCwd: String = ""
    private var serverHome: String = ""
    
    // MARK: - Properties
    weak var delegate: TerminalViewControllerDelegate?
    private var serverAddress: String = ""
    private let baseTopInset: CGFloat = 8
    
    /// Client core instance for C++ functionality
    var clientCore: ClientCoreWrapper?
    
    // Terminal size tracking
    private var lastTerminalCols: Int = 0
    private var lastTerminalRows: Int = 0
    private var resizeDebounceTimer: Timer?
    private let terminalFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
    
    // MARK: - Command Blocks Model (in-memory only, no UI yet)
    private struct CommandBlock {
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
    private var commandBlocks: [CommandBlock] = []
    
    // Pointer to current unfinished block (array index)
    private var currentBlockIndex: Int? = nil

    // MARK: - Global Document for unified selection (model only)
    private enum SegmentKind { case header, output }
    private struct GlobalSegment {
        let blockIndex: Int
        let kind: SegmentKind
        var range: NSRange // global range in combined document
    }
    private struct GlobalDocument {
        var totalLength: Int = 0
        var segments: [GlobalSegment] = []
    }
    private var globalDocument = GlobalDocument()

    // MARK: - Selection state (global)
    private var isSelecting: Bool = false
    private var selectionAnchor: Int? = nil // global index of selection start
    private var selectionRange: NSRange? = nil // current global range
    
    // MARK: - Autoscroll state
    private var autoscrollTimer: Timer?
    private var lastDragEvent: NSEvent?
    
    // MARK: - Raw Input Mode (when command is running)
    private var isCommandRunning: Bool = false
    
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
        
        setupTerminalView()
        setupInputView()
        
        // Add main views
        view.addSubview(terminalScrollView)
        view.addSubview(inputContainerView)
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
    
    private func updateTextViewFrame() {
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
        
        // Terminal view - занимает всё пространство от верха до input
        terminalScrollView.snp.makeConstraints { make in
            make.top.leading.trailing.equalToSuperview()
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
            make.top.leading.trailing.equalToSuperview()
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
    private func sendRawInput(_ text: String) {
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

// MARK: - TabHandlingTextFieldDelegate
extension TerminalViewController: TabHandlingTextFieldDelegate {
    func tabHandlingTextField(_ textField: TabHandlingTextField, didPressTabWithText text: String, cursorPosition: Int) {
        print("🎯 TerminalViewController received Tab event:")
        print("   Text: '\(text)'")
        print("   Cursor position: \(cursorPosition)")
        
        // Send completion request to server
        clientCore?.send(["type": "requestCompletion", "text": text, "cursorPosition": cursorPosition])
    }
}

// MARK: - Completion Logic
extension TerminalViewController {
    fileprivate func setupSelectionGestures() {
        // Перехватим события колллекции, чтобы мышь шла через VC
        collectionView.postsFrameChangedNotifications = true
        collectionView.acceptsTouchEvents = false
        // Включаем отслеживание мыши
        collectionView.addTrackingArea(NSTrackingArea(rect: collectionView.bounds, options: [.activeAlways, .mouseMoved, .mouseEnteredAndExited, .inVisibleRect], owner: self, userInfo: nil))
        
        // Жест нажатия (эмулирует mouseDown)
        let press = NSPressGestureRecognizer(target: self, action: #selector(handlePressGesture(_:)))
        press.minimumPressDuration = 0
        press.delegate = self
        collectionView.addGestureRecognizer(press)
        
        // Жест перетаскивания (эмулирует mouseDragged)
        let pan = NSPanGestureRecognizer(target: self, action: #selector(handlePanGesture(_:)))
        pan.delegate = self
        collectionView.addGestureRecognizer(pan)
    }
    // Пока просто фиксация событий, без изменения текста.
    func didStartCommandBlock(command: String? = nil, cwd: String? = nil) {
        print("🧱 Started command block: \(command ?? "<unknown>"), cwd: \(cwd ?? "<unknown>")")
        let block = CommandBlock(id: UUID(), command: command, outputSegments: [], isFinished: false, exitCode: nil, cwdStart: cwd, cwdEnd: nil)
        commandBlocks.append(block)
        currentBlockIndex = commandBlocks.count - 1
        insertBlock(at: currentBlockIndex!)
        rebuildGlobalDocument(startingAt: currentBlockIndex!)
        
        // Enter raw input mode for interactive commands
        enterRawInputMode()
    }
    
    func didFinishCommandBlock(exitCode: Int, cwd: String? = nil) {
        print("🏁 Finished command block (exit=\(exitCode)), cwd: \(cwd ?? "<unknown>")")
        if let idx = currentBlockIndex {
            commandBlocks[idx].isFinished = true
            commandBlocks[idx].exitCode = exitCode
            commandBlocks[idx].cwdEnd = cwd
            reloadBlock(at: idx)
            currentBlockIndex = nil
            rebuildGlobalDocument(startingAt: idx)
        }
        // Обновляем отображение cwd если он изменился (например после cd)
        if let newCwd = cwd {
            updateCurrentCwd(newCwd)
        }
        
        // Exit raw input mode
        exitRawInputMode()
    }
    
    /// Loads command history from server
    func loadHistory(_ history: [CommandHistoryRecord]) {
        print("📜 Loading history: \(history.count) commands")
        
        // Clear current state
        commandBlocks.removeAll()
        currentBlockIndex = nil
        globalDocument = GlobalDocument()
        selectionRange = nil
        selectionAnchor = nil
        
        // Create blocks from history
        // Note: history comes with raw output, wrap in unstyled segments for now
        // TODO: Server should send pre-parsed segments in history too
        for record in history {
            let segments = record.output.isEmpty ? [] : [StyledSegment(text: record.output, style: SegmentStyle())]
            let block = CommandBlock(
                id: UUID(),
                command: record.command.isEmpty ? nil : record.command,
                outputSegments: segments,
                isFinished: record.isFinished,
                exitCode: record.exitCode,
                cwdStart: record.cwdStart.isEmpty ? nil : record.cwdStart,
                cwdEnd: record.cwdEnd.isEmpty ? nil : record.cwdEnd
            )
            commandBlocks.append(block)
        }
        
        // Полная перезагрузка collectionView после обновления модели
        collectionView.reloadData()
        
        if !commandBlocks.isEmpty {
            rebuildGlobalDocument(startingAt: 0)
            
            // Check if last block is unfinished (running command)
            // If so, set currentBlockIndex to continue appending output to it
            if let lastIndex = commandBlocks.indices.last, !commandBlocks[lastIndex].isFinished {
                currentBlockIndex = lastIndex
                print("📜 Resuming unfinished command block at index \(lastIndex)")
                enterRawInputMode()
            }
            
            // Update CWD from last finished block
            if let lastBlock = commandBlocks.last {
                if let cwd = lastBlock.cwdEnd ?? lastBlock.cwdStart {
                    updateCurrentCwd(cwd)
                }
            }
            
            // Scroll to bottom
            DispatchQueue.main.async {
                self.updateTextViewFrame()
                self.scrollToBottom()
            }
        }
        
        print("📜 History loaded")
    }
    
    /// Handles completion results and applies them to input field
    func handleCompletionResults(_ completions: [String], originalText: String, cursorPosition: Int) {
        print("🎯 Processing completion:")
        print("   Original text: '\(originalText)'")
        print("   Cursor position: \(cursorPosition)")
        print("   Options: \(completions)")
        
        switch completions.count {
        case 0:
            // No completion options - insert literal tab
            handleNoCompletions(originalText: originalText, cursorPosition: cursorPosition)
            
        case 1:
            // Single option - auto-complete
            handleSingleCompletion(completions[0], originalText: originalText, cursorPosition: cursorPosition)
            
        default:
            // Multiple options - find common prefix or show list
            handleMultipleCompletions(completions, originalText: originalText, cursorPosition: cursorPosition)
        }
    }
    
    /// Handles case when there are no completion options - inserts literal tab
    private func handleNoCompletions(originalText: String, cursorPosition: Int) {
        print("⇥ No completion options - inserting tab")
        
        // Insert tab character at cursor position
        let beforeCursor = String(originalText.prefix(cursorPosition))
        let afterCursor = String(originalText.suffix(originalText.count - cursorPosition))
        let newText = beforeCursor + "\t" + afterCursor
        
        commandTextField.stringValue = newText
        
        // Move cursor after inserted tab
        let newCursorPosition = cursorPosition + 1
        if let editor = commandTextField.currentEditor() {
            editor.selectedRange = NSRange(location: newCursorPosition, length: 0)
        }
    }
    
    /// Handles case with single completion option
    private func handleSingleCompletion(_ completion: String, originalText: String, cursorPosition: Int) {
        print("✅ Single option: '\(completion)'")
        
        // Apply completion to input field
        applyCompletion(completion, originalText: originalText, cursorPosition: cursorPosition)
        
        showTemporaryMessage("Completed to: \(completion)")
    }
    
    /// Handles case with multiple completion options
    private func handleMultipleCompletions(_ completions: [String], originalText: String, cursorPosition: Int) {
        print("🔄 Multiple options (\(completions.count))")
        
        // Find common prefix among all options
        let commonPrefix = findCommonPrefix(completions)
        let currentWord = extractCurrentWord(originalText, cursorPosition: cursorPosition)
        
        print("   Current word: '\(currentWord)'")
        print("   Common prefix: '\(commonPrefix)'")
        
        if commonPrefix.count > currentWord.count {
            // There's a common prefix longer than current word - complete to it
            print("✅ Completing to common prefix: '\(commonPrefix)'")
            applyCompletion(commonPrefix, originalText: originalText, cursorPosition: cursorPosition)
            showTemporaryMessage("Completed to common prefix")
        } else {
            // No common prefix - show list of options
            print("📋 Showing options list")
            showCompletionList(completions)
        }
    }
    
    /// Applies completion to input field
    private func applyCompletion(_ completion: String, originalText: String, cursorPosition: Int) {
        // Extract current word to replace
        let currentWord = extractCurrentWord(originalText, cursorPosition: cursorPosition)
        let wordStart = findWordStart(originalText, cursorPosition: cursorPosition)
        
        // Create new text with replacement
        let beforeWord = String(originalText.prefix(wordStart))
        let afterCursor = String(originalText.suffix(originalText.count - cursorPosition))
        let newText = beforeWord + completion + afterCursor
        
        print("🔄 Applying completion:")
        print("   Before word: '\(beforeWord)'")
        print("   Replacing: '\(currentWord)' → '\(completion)'")
        print("   After cursor: '\(afterCursor)'")
        print("   Result: '\(newText)'")
        
        // Update input field
        commandTextField.stringValue = newText
        
        // Set cursor at end of completed word
        let newCursorPosition = beforeWord.count + completion.count
        setCursorPosition(newCursorPosition)
    }
    
    /// Extracts current word under cursor
    private func extractCurrentWord(_ text: String, cursorPosition: Int) -> String {
        let wordStart = findWordStart(text, cursorPosition: cursorPosition)
        let wordEnd = cursorPosition
        
        if wordStart < wordEnd && wordStart < text.count && wordEnd <= text.count {
            let startIndex = text.index(text.startIndex, offsetBy: wordStart)
            let endIndex = text.index(text.startIndex, offsetBy: wordEnd)
            return String(text[startIndex..<endIndex])
        }
        
        return ""
    }
    
    /// Finds start of current word
    private func findWordStart(_ text: String, cursorPosition: Int) -> Int {
        var start = cursorPosition - 1
        
        while start >= 0 && start < text.count {
            let index = text.index(text.startIndex, offsetBy: start)
            let char = text[index]
            
            if char == " " || char == "\t" {
                break
            }
            start -= 1
        }
        
        return start + 1
    }
    
    /// Finds common prefix among all completion options
    private func findCommonPrefix(_ completions: [String]) -> String {
        guard !completions.isEmpty else { return "" }
        guard completions.count > 1 else { return completions[0] }
        
        let first = completions[0]
        var commonLength = 0
        
        for i in 0..<first.count {
            let char = first[first.index(first.startIndex, offsetBy: i)]
            var allMatch = true
            
            for completion in completions.dropFirst() {
                if i >= completion.count || completion[completion.index(completion.startIndex, offsetBy: i)] != char {
                    allMatch = false
                    break
                }
            }
            
            if allMatch {
                commonLength = i + 1
            } else {
                break
            }
        }
        
        return String(first.prefix(commonLength))
    }
    
    /// Sets cursor position in input field
    private func setCursorPosition(_ position: Int) {
        if let fieldEditor = commandTextField.currentEditor() {
            let range = NSRange(location: position, length: 0)
            fieldEditor.selectedRange = range
        }
    }
    
    /// Shows list of completion options in terminal
    private func showCompletionList(_ completions: [String]) {
        let completionText = "💡 Completion options:\n" + completions.map { "  \($0)" }.joined(separator: "\n") + "\n"
        appendOutput(completionText)
    }
    
    /// Shows temporary message (logged to console)
    private func showTemporaryMessage(_ message: String) {
        print("💬 \(message)")
    }
}

// MARK: - Global Document rebuild
extension TerminalViewController {
    /// Completely rebuilds global segment map starting from specified block index.
    /// For simplicity, currently recalculating entire document.
    fileprivate func rebuildGlobalDocument(startingAt _: Int) {
        var segments: [GlobalSegment] = []
        var offset = 0
        for (idx, block) in commandBlocks.enumerated() {
            if let command = block.command {
                let cmdTextNSString = ("$ \(command)\n") as NSString
                let range = NSRange(location: offset, length: cmdTextNSString.length)
                segments.append(GlobalSegment(blockIndex: idx, kind: .header, range: range))
                offset += cmdTextNSString.length
            }

            if !block.outputText.isEmpty {
                let outNSString = block.outputText as NSString
                let range = NSRange(location: offset, length: outNSString.length)
                segments.append(GlobalSegment(blockIndex: idx, kind: .output, range: range))
                offset += outNSString.length
            }
        }
        globalDocument = GlobalDocument(totalLength: offset, segments: segments)
        // print("🧭 GlobalDocument rebuilt: length=\(globalDocument.totalLength), segments=\(globalDocument.segments.count)")
    }
}

// MARK: - Collection helpers
extension TerminalViewController: NSCollectionViewDataSource, NSCollectionViewDelegate, NSCollectionViewDelegateFlowLayout {
    func numberOfSections(in collectionView: NSCollectionView) -> Int { 1 }
    func collectionView(_ collectionView: NSCollectionView, numberOfItemsInSection section: Int) -> Int {
        return commandBlocks.count
    }

    func collectionView(_ collectionView: NSCollectionView, itemForRepresentedObjectAt indexPath: IndexPath) -> NSCollectionViewItem {
        let item = collectionView.makeItem(withIdentifier: CommandBlockItem.reuseId, for: indexPath)
        guard let blockItem = item as? CommandBlockItem else { return item }
        let block = commandBlocks[indexPath.item]
        blockItem.configure(command: block.command, outputSegments: block.outputSegments, isFinished: block.isFinished, exitCode: block.exitCode, cwdStart: block.cwdStart, serverHome: serverHome)
        // apply highlight for current selection if it intersects this block
        applySelectionHighlightIfNeeded(to: blockItem, at: indexPath.item)
        return blockItem
    }

    func collectionView(_ collectionView: NSCollectionView, layout collectionViewLayout: NSCollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> NSSize {
        let contentWidth = collectionView.bounds.width - (collectionLayout.sectionInset.left + collectionLayout.sectionInset.right)
        let block = commandBlocks[indexPath.item]
        let height = CommandBlockItem.estimatedHeight(command: block.command, outputText: block.outputText, width: contentWidth, cwdStart: block.cwdStart)
        return NSSize(width: contentWidth, height: height)
    }

    private func insertBlock(at index: Int) {
        let indexPath = IndexPath(item: index, section: 0)
        collectionView.performBatchUpdates({
            collectionView.insertItems(at: Set([indexPath]))
        }, completionHandler: { _ in
            self.updateTextViewFrame()
            self.scrollToBottom()
        })
    }

    private func reloadBlock(at index: Int) {
        let indexPath = IndexPath(item: index, section: 0)
        collectionView.reloadItems(at: Set([indexPath]))
        self.updateTextViewFrame()
        scrollToBottomThrottled()
    }

    private func scrollToBottom() {
        let count = collectionView.numberOfItems(inSection: 0)
        if count > 0 {
            let indexPath = IndexPath(item: count - 1, section: 0)
            collectionView.scrollToItems(at: Set([indexPath]), scrollPosition: .bottom)
        }
    }

    private var lastScrollUpdate: TimeInterval { get { _lastScrollUpdate } set { _lastScrollUpdate = newValue } }
    private static var _scrollTimestamp: TimeInterval = 0
    private var _lastScrollUpdate: TimeInterval {
        get { return TerminalViewController._scrollTimestamp }
        set { TerminalViewController._scrollTimestamp = newValue }
    }
    private func scrollToBottomThrottled() {
        let now = CFAbsoluteTimeGetCurrent()
        if now - lastScrollUpdate > 0.03 {
            lastScrollUpdate = now
            scrollToBottom()
        }
    }
}

// MARK: - Selection handling & highlighting
extension TerminalViewController {
    override func mouseDown(with event: NSEvent) {
        guard view.window != nil else { return }
        let locationInView = view.convert(event.locationInWindow, from: nil)
        guard let (_, localIndex) = hitTestGlobalIndex(at: locationInView) else { return }
        let globalIndex = localIndex
        isSelecting = true
        selectionAnchor = globalIndex
        selectionRange = NSRange(location: globalIndex, length: 0)
        updateSelectionHighlight()
    }

    override func mouseDragged(with event: NSEvent) {
        guard isSelecting, let anchor = selectionAnchor else { return }
        let locationInView = view.convert(event.locationInWindow, from: nil)
        
        // Autoscroll if cursor is outside scroll view bounds
        let scrollBounds = terminalScrollView.convert(terminalScrollView.bounds, to: view)
        handleAutoscroll(locationInView: locationInView, scrollBounds: scrollBounds, event: event)
        
        // Get global index - use edge detection if outside content
        let globalIndex: Int
        if let (_, idx) = hitTestGlobalIndex(at: locationInView) {
            globalIndex = idx
        } else {
            // Cursor is outside content - select to edge
            globalIndex = getEdgeGlobalIndex(locationInView: locationInView, scrollBounds: scrollBounds)
        }
        
        let start = min(anchor, globalIndex)
        let end = max(anchor, globalIndex)
        selectionRange = NSRange(location: start, length: end - start)
        updateSelectionHighlight()
    }

    override func mouseUp(with event: NSEvent) {
        isSelecting = false
        stopAutoscrollTimer()
    }
    
    // MARK: - Autoscroll Support
    
    private func handleAutoscroll(locationInView: NSPoint, scrollBounds: NSRect, event: NSEvent) {
        lastDragEvent = event
        
        // Calculate distance outside scroll bounds
        var deltaY: CGFloat = 0
        if locationInView.y < scrollBounds.minY {
            deltaY = locationInView.y - scrollBounds.minY // negative = scroll down (content moves up)
        } else if locationInView.y > scrollBounds.maxY {
            deltaY = locationInView.y - scrollBounds.maxY // positive = scroll up (content moves down)
        }
        
        if abs(deltaY) > 0 {
            // Start autoscroll timer if not running
            if autoscrollTimer == nil {
                autoscrollTimer = Timer.scheduledTimer(withTimeInterval: 0.02, repeats: true) { [weak self] _ in
                    self?.performAutoscroll()
                }
            }
        } else {
            stopAutoscrollTimer()
        }
    }
    
    private func performAutoscroll() {
        guard isSelecting, let event = lastDragEvent else {
            stopAutoscrollTimer()
            return
        }
        
        let locationInView = view.convert(event.locationInWindow, from: nil)
        let scrollBounds = terminalScrollView.convert(terminalScrollView.bounds, to: view)
        
        // Calculate scroll speed proportional to distance
        var deltaY: CGFloat = 0
        if locationInView.y < scrollBounds.minY {
            deltaY = (scrollBounds.minY - locationInView.y) * 0.5 // scroll down
        } else if locationInView.y > scrollBounds.maxY {
            deltaY = (scrollBounds.maxY - locationInView.y) * 0.5 // scroll up (negative)
        }
        
        if abs(deltaY) < 1 {
            stopAutoscrollTimer()
            return
        }
        
        // Apply scroll
        let clipView = terminalScrollView.contentView
        var newOrigin = clipView.bounds.origin
        newOrigin.y = max(0, min(newOrigin.y + deltaY, collectionView.frame.height - clipView.bounds.height))
        clipView.setBoundsOrigin(newOrigin)
        terminalScrollView.reflectScrolledClipView(clipView)
        
        // Update selection with new scroll position
        if let anchor = selectionAnchor {
            let globalIndex: Int
            if let (_, idx) = hitTestGlobalIndex(at: locationInView) {
                globalIndex = idx
            } else {
                globalIndex = getEdgeGlobalIndex(locationInView: locationInView, scrollBounds: scrollBounds)
            }
            let start = min(anchor, globalIndex)
            let end = max(anchor, globalIndex)
            selectionRange = NSRange(location: start, length: end - start)
            updateSelectionHighlight()
        }
    }
    
    private func stopAutoscrollTimer() {
        autoscrollTimer?.invalidate()
        autoscrollTimer = nil
    }
    
    /// Returns global index at document edge when cursor is outside content
    private func getEdgeGlobalIndex(locationInView: NSPoint, scrollBounds: NSRect) -> Int {
        if locationInView.y < scrollBounds.minY {
            // Below scroll view (in flipped coordinates this means end of document)
            return globalDocument.totalLength
        } else if locationInView.y > scrollBounds.maxY {
            // Above scroll view (beginning of document)
            return 0
        }
        // Horizontal edges - find nearest visible line
        return selectionAnchor ?? 0
    }

    override func keyDown(with event: NSEvent) {
        // Cmd+C — copy selected text (works in both modes)
        if event.modifierFlags.contains(.command), let chars = event.charactersIgnoringModifiers, chars.lowercased() == "c" {
            copySelectionToPasteboard()
            return
        }
        
        // Cmd+V — paste (in raw mode, send to PTY)
        if event.modifierFlags.contains(.command), let chars = event.charactersIgnoringModifiers, chars.lowercased() == "v" {
            if isCommandRunning {
                if let pasteString = NSPasteboard.general.string(forType: .string) {
                    sendRawInput(pasteString)
                }
                return
            }
        }
        
        // Raw input mode: send all keypresses to PTY
        if isCommandRunning {
            handleRawKeyDown(event)
            return
        }
        
        super.keyDown(with: event)
    }
    
    /// Handles key press in raw input mode
    private func handleRawKeyDown(_ event: NSEvent) {
        let keyCode = event.keyCode
        let modifiers = event.modifierFlags
        
        // Handle special keys
        switch keyCode {
        case 36: // Enter/Return
            sendRawInput("\n")
            return
        case 51: // Backspace
            sendRawInput("\u{7f}") // DEL character
            return
        case 53: // Escape
            sendRawInput("\u{1b}")
            return
        case 48: // Tab
            sendRawInput("\t")
            return
        case 123: // Left arrow
            sendRawInput("\u{1b}[D")
            return
        case 124: // Right arrow
            sendRawInput("\u{1b}[C")
            return
        case 125: // Down arrow
            sendRawInput("\u{1b}[B")
            return
        case 126: // Up arrow
            sendRawInput("\u{1b}[A")
            return
        case 115: // Home
            sendRawInput("\u{1b}[H")
            return
        case 119: // End
            sendRawInput("\u{1b}[F")
            return
        case 116: // Page Up
            sendRawInput("\u{1b}[5~")
            return
        case 121: // Page Down
            sendRawInput("\u{1b}[6~")
            return
        case 117: // Delete (forward)
            sendRawInput("\u{1b}[3~")
            return
        default:
            break
        }
        
        // Ctrl+key combinations
        if modifiers.contains(.control), let chars = event.charactersIgnoringModifiers {
            if let char = chars.first {
                let asciiValue = char.asciiValue ?? 0
                // Ctrl+A = 1, Ctrl+B = 2, ..., Ctrl+Z = 26
                if asciiValue >= 97 && asciiValue <= 122 { // a-z
                    let ctrlChar = Character(UnicodeScalar(asciiValue - 96))
                    sendRawInput(String(ctrlChar))
                    return
                }
                // Ctrl+C specifically
                if char == "c" {
                    sendRawInput("\u{03}") // ETX (Ctrl+C)
                    return
                }
                // Ctrl+D
                if char == "d" {
                    sendRawInput("\u{04}") // EOT (Ctrl+D)
                    return
                }
                // Ctrl+Z
                if char == "z" {
                    sendRawInput("\u{1a}") // SUB (Ctrl+Z)
                    return
                }
            }
        }
        
        // Regular characters
        if let chars = event.characters, !chars.isEmpty {
            sendRawInput(chars)
        }
    }

    /// Converts click coordinate to global character index if it hits text
    private func hitTestGlobalIndex(at pointInRoot: NSPoint) -> (blockIndex: Int, globalIndex: Int)? {
        // Iterate through visible items
        let visible = collectionView.visibleItems()
        for case let item as CommandBlockItem in visible {
            guard let indexPath = collectionView.indexPath(for: item) else { continue }
            // Convert point to item coordinates
            let pointInItem = item.view.convert(pointInRoot, from: view)
            if !item.view.bounds.contains(pointInItem) { continue }

            // Check header
            if let hIdx = item.headerCharacterIndex(at: pointInItem) {
                let global = mapLocalToGlobal(blockIndex: indexPath.item, kind: .header, localIndex: hIdx)
                return (indexPath.item, global)
            }
            // Check body
            if let bIdx = item.bodyCharacterIndex(at: pointInItem) {
                let global = mapLocalToGlobal(blockIndex: indexPath.item, kind: .output, localIndex: bIdx)
                return (indexPath.item, global)
            }
        }
        return nil
    }

    /// Converts local character index within block to global document index
    private func mapLocalToGlobal(blockIndex: Int, kind: SegmentKind, localIndex: Int) -> Int {
        for seg in globalDocument.segments {
            if seg.blockIndex == blockIndex && seg.kind == kind {
                return seg.range.location + min(localIndex, seg.range.length)
            }
        }
        // if segment not found — return end of document
        return globalDocument.totalLength
    }

    /// Highlights current selection in all visible cells
    private func updateSelectionHighlight() {
        guard let sel = selectionRange else {
            // clear highlight
            for case let item as CommandBlockItem in collectionView.visibleItems() {
                item.clearSelectionHighlight()
            }
            return
        }
        for case let item as CommandBlockItem in collectionView.visibleItems() {
            guard let indexPath = collectionView.indexPath(for: item) else { continue }
            let headerLocal = localRange(for: sel, blockIndex: indexPath.item, kind: .header)
            let bodyLocal = localRange(for: sel, blockIndex: indexPath.item, kind: .output)
            item.setSelectionHighlight(headerRange: headerLocal, bodyRange: bodyLocal)
        }
    }

    /// Returns local range within specified segment for global selection range
    private func localRange(for global: NSRange, blockIndex: Int, kind: SegmentKind) -> NSRange? {
        guard let seg = globalDocument.segments.first(where: { $0.blockIndex == blockIndex && $0.kind == kind }) else { return nil }
        let inter = intersection(of: global, and: seg.range)
        guard inter.length > 0 else { return nil }
        return NSRange(location: inter.location - seg.range.location, length: inter.length)
    }

    private func intersection(of a: NSRange, and b: NSRange) -> NSRange {
        let start = max(a.location, b.location)
        let end = min(a.location + a.length, b.location + b.length)
        return end > start ? NSRange(location: start, length: end - start) : NSRange(location: 0, length: 0)
    }

    /// Applies highlight when configuring cell
    fileprivate func applySelectionHighlightIfNeeded(to item: CommandBlockItem, at blockIndex: Int) {
        guard let sel = selectionRange else {
            item.clearSelectionHighlight(); return
        }
        let headerLocal = localRange(for: sel, blockIndex: blockIndex, kind: .header)
        let bodyLocal = localRange(for: sel, blockIndex: blockIndex, kind: .output)
        item.setSelectionHighlight(headerRange: headerLocal, bodyRange: bodyLocal)
    }

    private func copySelectionToPasteboard() {
        guard let sel = selectionRange, sel.length > 0 else { return }
        var result = ""
        for seg in globalDocument.segments {
            let inter = intersection(of: sel, and: seg.range)
            guard inter.length > 0 else { continue }
            let local = NSRange(location: inter.location - seg.range.location, length: inter.length)
            let block = commandBlocks[seg.blockIndex]
            switch seg.kind {
            case .header:
                let ns = ("$ \(block.command ?? "")\n") as NSString
                if local.location < ns.length, local.length > 0, local.location + local.length <= ns.length {
                    var piece = ns.substring(with: local)
                    if piece.hasPrefix("$ ") { piece.removeFirst(2) }
                    result += piece
                }
            case .output:
                let ns = (block.outputText as NSString)
                if local.location < ns.length, local.length > 0, local.location + local.length <= ns.length {
                    result += ns.substring(with: local)
                }
            }
        }
        let pb = NSPasteboard.general
        pb.clearContents()
        pb.setString(result, forType: .string)
    }
}

// MARK: - Gesture handlers
extension TerminalViewController {
    @objc private func handlePressGesture(_ gr: NSPressGestureRecognizer) {
        guard let v = gr.view else { return }
        let p = view.convert(gr.location(in: v), from: v)
        switch gr.state {
        case .began:
            // Назначаем себя firstResponder, чтобы перехватывать Cmd+C
            view.window?.makeFirstResponder(self)
            if let (_, gi) = hitTestGlobalIndex(at: p) {
                isSelecting = true
                selectionAnchor = gi
                selectionRange = NSRange(location: gi, length: 0)
                updateSelectionHighlight()
            }
        default:
            break
        }
    }
    
    @objc private func handlePanGesture(_ gr: NSPanGestureRecognizer) {
        guard let v = gr.view else { return }
        let p = view.convert(gr.location(in: v), from: v)
        switch gr.state {
        case .began:
            // Назначаем себя firstResponder, чтобы перехватывать Cmd+C
            view.window?.makeFirstResponder(self)
            if let (_, gi) = hitTestGlobalIndex(at: p) {
                isSelecting = true
                selectionAnchor = gi
                selectionRange = NSRange(location: gi, length: 0)
                updateSelectionHighlight()
            }
        case .changed:
            guard isSelecting, let anchor = selectionAnchor, let (_, gi) = hitTestGlobalIndex(at: p) else { return }
            let start = min(anchor, gi)
            let end = max(anchor, gi)
            selectionRange = NSRange(location: start, length: end - start)
            updateSelectionHighlight()
        case .ended, .cancelled, .failed:
            isSelecting = false
        default:
            break
        }
    }
}

// MARK: - Standard Edit Actions
extension TerminalViewController {
    @IBAction func copy(_ sender: Any?) {
        if let sel = selectionRange, sel.length > 0 {
            copySelectionToPasteboard()
        } else {
            NSSound.beep()
        }
    }
}

// MARK: - NSGestureRecognizerDelegate
extension TerminalViewController {
    func gestureRecognizer(_ gestureRecognizer: NSGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: NSGestureRecognizer) -> Bool {
        return true
    }
}

// MARK: - Delegate Protocol
protocol TerminalViewControllerDelegate: AnyObject {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String)
    func terminalViewControllerDidRequestDisconnect(_ controller: TerminalViewController)
}

// MARK: - Character Extension
private extension Character {
    /// Returns true if character is printable (not a control character)
    var isPrintable: Bool {
        // Control characters are 0x00-0x1F and 0x7F
        guard let scalar = unicodeScalars.first else { return false }
        let value = scalar.value
        return value >= 0x20 && value != 0x7F
    }
}
