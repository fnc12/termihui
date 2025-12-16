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
    
    // Current working directory
    private var currentCwd: String = ""
    
    // MARK: - Properties
    weak var delegate: TerminalViewControllerDelegate?
    weak var webSocketManager: WebSocketManager?
    private var serverAddress: String = ""
    private let ansiParser = ANSIParser()
    private let baseTopInset: CGFloat = 8
    
    // Terminal size tracking
    private var lastTerminalCols: Int = 0
    private var lastTerminalRows: Int = 0
    private var resizeDebounceTimer: Timer?
    private let terminalFont = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
    
    // MARK: - Command Blocks Model (in-memory only, no UI yet)
    private struct CommandBlock {
        let id: UUID
        var command: String?
        var output: String
        var isFinished: Bool
        var exitCode: Int?
        var cwdStart: String?   // cwd when command started
        var cwdEnd: String?     // cwd after command finished (can change, e.g. cd)
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
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupLayout()
        setupActions()
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
        // Get the width of the output area (collectionView)
        let viewWidth = terminalScrollView.contentSize.width
        let viewHeight = terminalScrollView.contentSize.height
        
        // Account for padding/margins in CommandBlockItem
        let effectiveWidth = viewWidth - (CommandBlockItem.horizontalPadding * 2)
        
        // Calculate character dimensions
        let charSize = "M".size(withAttributes: [.font: terminalFont])
        let charWidth = charSize.width
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
            webSocketManager?.sendResize(cols: cols, rows: rows)
        }
    }
    
    /// Force send current terminal size (e.g., on initial connection)
    func sendInitialTerminalSize() {
        let (cols, rows) = calculateTerminalSize()
        lastTerminalCols = cols
        lastTerminalRows = rows
        
        print("📐 Initial terminal size: \(cols)x\(rows)")
        webSocketManager?.sendResize(cols: cols, rows: rows)
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
        inputContainerView.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
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
        
        // Command text field
        commandTextField.placeholderString = "Введите команду..."
        commandTextField.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        commandTextField.target = self
        commandTextField.action = #selector(sendCommand)
        commandTextField.tabDelegate = self // Устанавливаем делегат для Tab-обработки
        
        // Убираем все визуальные элементы поля для слияния с фоном
        commandTextField.focusRingType = .none
        commandTextField.isBordered = false
        commandTextField.isBezeled = false
        commandTextField.backgroundColor = NSColor.clear
        commandTextField.drawsBackground = false
        
        // Добавляем тонкую линию снизу как в современных терминалах (опционально)
        let underlineView = NSView()
        underlineView.wantsLayer = true
        underlineView.layer?.backgroundColor = NSColor.separatorColor.cgColor
        inputContainerView.addSubview(underlineView)
        
        // Сохраняем ссылку для layout constraints
        self.inputUnderlineView = underlineView
        
        // Send button
        sendButton.bezelStyle = .rounded
        sendButton.controlSize = .regular
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
            make.bottom.equalTo(inputContainerView.snp.top)
            make.height.greaterThanOrEqualTo(200)
        }
        
        print("🔧 Terminal constraints set with minimum height 200")
        
        // Input container
        inputContainerView.snp.makeConstraints { make in
            make.leading.trailing.equalToSuperview()
            make.bottom.equalToSuperview() // Прижимаем к низу
            make.height.equalTo(70) // Увеличиваем высоту для cwd лейбла
        }
        
        // CWD label сверху
        cwdLabel.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.trailing.equalToSuperview().offset(-12)
            make.top.equalToSuperview().offset(6)
            make.height.equalTo(16)
        }
        
        commandTextField.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.top.equalTo(cwdLabel.snp.bottom).offset(4)
            make.trailing.equalTo(sendButton.snp.leading).offset(-8)
            make.height.equalTo(24)
        }
        
        sendButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-12)
            make.centerY.equalTo(commandTextField)
            make.width.equalTo(80)
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
    
    private func setupActions() {
        // Actions already set in setup methods
    }
    
    // MARK: - Public Methods
    func configure(serverAddress: String) {
        self.serverAddress = serverAddress
        // Устанавливаем заголовок окна
        view.window?.title = "TermiHUI — \(serverAddress)"
    }
    
    func appendOutput(_ output: String) {
        print("📺 TerminalViewController.appendOutput called with: *\(output)*")
        // Копим вывод в текущем блоке (если есть незавершённый)
        if let idx = currentBlockIndex {
            commandBlocks[idx].output.append(output)
            reloadBlock(at: idx)
            rebuildGlobalDocument(startingAt: idx)
        } else {
            // Если блока нет (например, вывод вне команды) — создаём самостоятельный блок
            let block = CommandBlock(id: UUID(), command: nil, output: output, isFinished: false, exitCode: nil, cwdStart: nil, cwdEnd: nil)
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
    
    /// Обновляет отображение текущей рабочей директории
    func updateCurrentCwd(_ cwd: String) {
        currentCwd = cwd
        // Получаем реальный home directory (не sandboxed)
        let homeDir = realHomeDirectory()
        let displayCwd: String
        if cwd.hasPrefix(homeDir) {
            displayCwd = "~" + String(cwd.dropFirst(homeDir.count))
        } else {
            displayCwd = cwd
        }
        cwdLabel.stringValue = displayCwd
        print("📂 CWD updated: \(displayCwd)")
    }
    
    /// Возвращает реальный home directory пользователя (не sandboxed контейнер)
    private func realHomeDirectory() -> String {
        // NSHomeDirectory() в sandboxed app возвращает путь к контейнеру
        // Используем getpwuid для получения реального home
        if let pw = getpwuid(getuid()), let home = pw.pointee.pw_dir {
            return String(cString: home)
        }
        // Fallback: извлекаем из /Users/username
        let components = NSHomeDirectory().components(separatedBy: "/")
        if components.count >= 3 && components[1] == "Users" {
            return "/Users/\(components[2])"
        }
        return NSHomeDirectory()
    }
    
    // MARK: - Actions
    @objc private func sendCommand() {
        let command = commandTextField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
        
        guard !command.isEmpty else { return }
        
        // Очищаем поле ввода
        commandTextField.stringValue = ""
        
        // НЕ добавляем эхо команды - PTY уже предоставляет полный вывод
        // appendOutput("$ \(command)\n")  // Убираем дублирование
        
        // Отправляем команду через delegate
        delegate?.terminalViewController(self, didSendCommand: command)
    }
    
    /// Вызывается из меню Client -> Disconnect
    func requestDisconnect() {
        delegate?.terminalViewControllerDidRequestDisconnect(self)
    }
}

// MARK: - TabHandlingTextFieldDelegate
extension TerminalViewController: TabHandlingTextFieldDelegate {
    func tabHandlingTextField(_ textField: TabHandlingTextField, didPressTabWithText text: String, cursorPosition: Int) {
        print("🎯 TerminalViewController received Tab event:")
        print("   Text: '\(text)'")
        print("   Cursor position: \(cursorPosition)")
        
        // Send completion request to server
        webSocketManager?.requestCompletion(for: text, cursorPosition: cursorPosition)
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
        let block = CommandBlock(id: UUID(), command: command, output: "", isFinished: false, exitCode: nil, cwdStart: cwd, cwdEnd: nil)
        commandBlocks.append(block)
        currentBlockIndex = commandBlocks.count - 1
        insertBlock(at: currentBlockIndex!)
        rebuildGlobalDocument(startingAt: currentBlockIndex!)
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
    }
    
    /// Loads command history from server
    func loadHistory(_ history: [CommandHistoryRecord]) {
        print("📜 Loading history: \(history.count) commands")
        
        // Clear current blocks
        commandBlocks.removeAll()
        currentBlockIndex = nil
        globalDocument = GlobalDocument()
        selectionRange = nil
        selectionAnchor = nil
        
        // Reload collectionView
        collectionView.reloadData()
        
        // Create blocks from history
        for record in history {
            let block = CommandBlock(
                id: UUID(),
                command: record.command.isEmpty ? nil : record.command,
                output: record.output,
                isFinished: record.isFinished,
                exitCode: record.exitCode,
                cwdStart: record.cwdStart.isEmpty ? nil : record.cwdStart,
                cwdEnd: record.cwdEnd.isEmpty ? nil : record.cwdEnd
            )
            commandBlocks.append(block)
        }
        
        // Insert all blocks and update collectionView
        if !commandBlocks.isEmpty {
            let indexPaths = (0..<commandBlocks.count).map { IndexPath(item: $0, section: 0) }
            collectionView.insertItems(at: Set(indexPaths))
            rebuildGlobalDocument(startingAt: 0)
            
            // Update CWD from last finished block
            if let lastBlock = commandBlocks.last {
                if let cwd = lastBlock.cwdEnd ?? lastBlock.cwdStart {
                    updateCurrentCwd(cwd)
                }
            }
            
            // Scroll to bottom
            DispatchQueue.main.async {
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

            if !block.output.isEmpty {
                let outNSString = block.output as NSString
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
        blockItem.configure(command: block.command, output: block.output, isFinished: block.isFinished, exitCode: block.exitCode, cwdStart: block.cwdStart)
        // apply highlight for current selection if it intersects this block
        applySelectionHighlightIfNeeded(to: blockItem, at: indexPath.item)
        return blockItem
    }

    func collectionView(_ collectionView: NSCollectionView, layout collectionViewLayout: NSCollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> NSSize {
        let contentWidth = collectionView.bounds.width - (collectionLayout.sectionInset.left + collectionLayout.sectionInset.right)
        let block = commandBlocks[indexPath.item]
        let height = CommandBlockItem.estimatedHeight(command: block.command, output: block.output, width: contentWidth, cwdStart: block.cwdStart)
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
        guard let window = view.window else { return }
        let locationInView = view.convert(event.locationInWindow, from: nil)
        guard let (blockIndex, localIndex) = hitTestGlobalIndex(at: locationInView) else { return }
        let globalIndex = localIndex
        isSelecting = true
        selectionAnchor = globalIndex
        selectionRange = NSRange(location: globalIndex, length: 0)
        updateSelectionHighlight()
    }

    override func mouseDragged(with event: NSEvent) {
        guard isSelecting, let anchor = selectionAnchor, let window = view.window else { return }
        let locationInView = view.convert(event.locationInWindow, from: nil)
        guard let (_, globalIndex) = hitTestGlobalIndex(at: locationInView) else { return }
        let start = min(anchor, globalIndex)
        let end = max(anchor, globalIndex)
        selectionRange = NSRange(location: start, length: end - start)
        updateSelectionHighlight()
    }

    override func mouseUp(with event: NSEvent) {
        isSelecting = false
    }

    override func keyDown(with event: NSEvent) {
        // Cmd+C — copy selected text
        if event.modifierFlags.contains(.command), let chars = event.charactersIgnoringModifiers, chars.lowercased() == "c" {
            copySelectionToPasteboard()
            return
        }
        super.keyDown(with: event)
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
                let ns = (block.output as NSString)
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
