import Cocoa
import SnapKit

/// Основной экран терминала
class TerminalViewController: NSViewController, NSGestureRecognizerDelegate {
    
    // MARK: - UI Components
    private let toolbarView = NSView()
    private let disconnectButton = NSButton(title: "Отключиться", target: nil, action: nil)
    private let statusLabel = NSTextField(labelWithString: "Подключен")
    
    private let terminalScrollView = NSScrollView()
    private var collectionView = NSCollectionView()
    private let collectionLayout = NSCollectionViewFlowLayout()
    
    private let inputContainerView = NSView()
    private let commandTextField = TabHandlingTextField()
    private let sendButton = NSButton(title: "Отправить", target: nil, action: nil)
    private var inputUnderlineView: NSView!
    
    // MARK: - Properties
    weak var delegate: TerminalViewControllerDelegate?
    weak var webSocketManager: WebSocketManager?
    private var serverAddress: String = ""
    private let ansiParser = ANSIParser()
    private let baseTopInset: CGFloat = 8
    
    // MARK: - Command Blocks Model (in-memory only, no UI yet)
    private struct CommandBlock {
        let id: UUID
        var command: String?
        var output: String
        var isFinished: Bool
        var exitCode: Int?
    }
    private var commandBlocks: [CommandBlock] = []
    
    // Указатель на текущий незавершённый блок (индекс в массиве)
    private var currentBlockIndex: Int? = nil

    // MARK: - Global Document for unified selection (model only)
    private enum SegmentKind { case header, output }
    private struct GlobalSegment {
        let blockIndex: Int
        let kind: SegmentKind
        var range: NSRange // глобальный диапазон в общем документе
    }
    private struct GlobalDocument {
        var totalLength: Int = 0
        var segments: [GlobalSegment] = []
    }
    private var globalDocument = GlobalDocument()

    // MARK: - Selection state (global)
    private var isSelecting: Bool = false
    private var selectionAnchor: Int? = nil // глобальный индекс начала выделения
    private var selectionRange: NSRange? = nil // текущий глобальный диапазон
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupLayout()
        setupActions()
    }
    
    override func viewDidAppear() {
        super.viewDidAppear()
        
        // Принудительно обновляем layout родительского view
        view.superview?.layoutSubtreeIfNeeded()
        view.layoutSubtreeIfNeeded()
        
        print("🔧 viewDidAppear: Parent view размер: \(view.frame)")
        print("🔧 viewDidAppear: ScrollView размер после layout: \(terminalScrollView.frame)")
        
        // Небольшая задержка чтобы layout точно завершился
        DispatchQueue.main.async {
            // ПЕРЕСОЗДАЁМ NSTextView ПОСЛЕ того как layout завершён
            self.recreateTextViewWithCorrectSize()
            
            // Устанавливаем фокус на поле ввода команд
            self.view.window?.makeFirstResponder(self.commandTextField)
        }
    }
    
    override func viewDidLayout() {
        super.viewDidLayout()
        
        // При изменении размера view обновляем frame NSTextView
        updateTextViewFrame()
    }
    
    // MARK: - Setup Methods
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.controlBackgroundColor.cgColor
        
        setupToolbar()
        setupTerminalView()
        setupInputView()
        
        // Add main views
        view.addSubview(toolbarView)
        view.addSubview(terminalScrollView)
        view.addSubview(inputContainerView)
    }
    
    private func setupToolbar() {
        toolbarView.wantsLayer = true
        toolbarView.layer?.backgroundColor = NSColor.separatorColor.cgColor
        
        // Disconnect button
        disconnectButton.bezelStyle = .rounded
        disconnectButton.controlSize = .small
        disconnectButton.target = self
        disconnectButton.action = #selector(disconnectButtonTapped)
        
        // Status label
        statusLabel.font = NSFont.systemFont(ofSize: 12)
        statusLabel.textColor = .secondaryLabelColor
        statusLabel.alignment = .right
        
        toolbarView.addSubview(disconnectButton)
        toolbarView.addSubview(statusLabel)
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

        // Устанавливаем стартовый frame коллекции вручную под текущий contentSize scrollView
        collectionView.frame = NSRect(origin: .zero, size: terminalScrollView.contentSize)

        print("🔧 CollectionView включён. TerminalScrollView размер: \(terminalScrollView.frame)")

        // Жесты для сквозного выделения
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
        print("🔧 setupLayout: Размеры до constraints:")
        print("   View: \(view.frame)")
        print("   ScrollView: \(terminalScrollView.frame)")
        print("   InputContainer: \(inputContainerView.frame)")
        
        // Toolbar
        toolbarView.snp.makeConstraints { make in
            make.top.leading.trailing.equalToSuperview()
            make.height.equalTo(40)
        }
        
        disconnectButton.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.centerY.equalToSuperview()
        }
        
        statusLabel.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-12)
            make.centerY.equalToSuperview()
            make.leading.greaterThanOrEqualTo(disconnectButton.snp.trailing).offset(12)
        }
        
        // Принудительно обновляем layout перед установкой constraints
        view.layoutSubtreeIfNeeded()
        
        // Terminal view - возвращаемся к старому подходу с минимальной высотой
        terminalScrollView.snp.makeConstraints { make in
            make.top.equalTo(toolbarView.snp.bottom)
            make.leading.trailing.equalToSuperview()
            make.bottom.equalTo(inputContainerView.snp.top)
            make.height.greaterThanOrEqualTo(200)
        }
        
        print("🔧 Terminal constraints установлены на весь размер: \(view.frame)")
        
        print("🔧 Terminal constraints установлены с минимальной высотой 200")
        
        // Input container
        inputContainerView.snp.makeConstraints { make in
            make.leading.trailing.equalToSuperview()
            make.bottom.equalToSuperview() // Прижимаем к низу
            make.height.equalTo(50)
        }
        
        commandTextField.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(12)
            make.centerY.equalToSuperview()
            make.trailing.equalTo(sendButton.snp.leading).offset(-8)
            make.height.equalTo(24)
        }
        
        sendButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-12)
            make.centerY.equalToSuperview()
            make.width.equalTo(80)
        }
        
        // Underline view constraints
        inputUnderlineView.snp.makeConstraints { make in
            make.leading.equalTo(commandTextField.snp.leading)
            make.trailing.equalTo(commandTextField.snp.trailing)
            make.bottom.equalTo(commandTextField.snp.bottom).offset(2)
            make.height.equalTo(1)
        }
        
        print("🔧 setupLayout завершён: все constraints установлены")
    }
    
    private func setupActions() {
        // Actions already set in setup methods
    }
    
    // MARK: - Public Methods
    func configure(serverAddress: String) {
        self.serverAddress = serverAddress
        statusLabel.stringValue = "Подключен к \(serverAddress)"
    }
    
    func appendOutput(_ output: String) {
        print("📺 TerminalViewController.appendOutput вызван с: *\(output)*")
        // Копим вывод в текущем блоке (если есть незавершённый)
        if let idx = currentBlockIndex {
            commandBlocks[idx].output.append(output)
            reloadBlock(at: idx)
            rebuildGlobalDocument(startingAt: idx)
        } else {
            // Если блока нет (например, вывод вне команды) — создаём самостоятельный блок
            let block = CommandBlock(id: UUID(), command: nil, output: output, isFinished: false, exitCode: nil)
            commandBlocks.append(block)
            let newIndex = commandBlocks.count - 1
            insertBlock(at: newIndex)
            currentBlockIndex = newIndex
            rebuildGlobalDocument(startingAt: newIndex)
        }
    }
    
    func showConnectionStatus(_ status: String) {
        statusLabel.stringValue = status
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
    
    @objc private func disconnectButtonTapped() {
        delegate?.terminalViewControllerDidRequestDisconnect(self)
    }
}

// MARK: - TabHandlingTextFieldDelegate
extension TerminalViewController: TabHandlingTextFieldDelegate {
    func tabHandlingTextField(_ textField: TabHandlingTextField, didPressTabWithText text: String, cursorPosition: Int) {
        print("🎯 TerminalViewController получил Tab событие:")
        print("   Текст: '\(text)'")
        print("   Позиция курсора: \(cursorPosition)")
        
        // Отправляем запрос автодополнения на сервер
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
    func didStartCommandBlock(command: String? = nil) {
        print("🧱 Начат блок команды: \(command ?? "<unknown>")")
        let block = CommandBlock(id: UUID(), command: command, output: "", isFinished: false, exitCode: nil)
        commandBlocks.append(block)
        currentBlockIndex = commandBlocks.count - 1
        insertBlock(at: currentBlockIndex!)
        rebuildGlobalDocument(startingAt: currentBlockIndex!)
    }
    
    func didFinishCommandBlock(exitCode: Int) {
        print("🏁 Завершён блок команды (exit=\(exitCode))")
        if let idx = currentBlockIndex {
            commandBlocks[idx].isFinished = true
            commandBlocks[idx].exitCode = exitCode
            reloadBlock(at: idx)
            currentBlockIndex = nil
            rebuildGlobalDocument(startingAt: idx)
        }
    }
    
    /// Обрабатывает результаты автодополнения и применяет их к полю ввода
    func handleCompletionResults(_ completions: [String], originalText: String, cursorPosition: Int) {
        print("🎯 Обработка автодополнения:")
        print("   Исходный текст: '\(originalText)'")
        print("   Позиция курсора: \(cursorPosition)")
        print("   Варианты: \(completions)")
        
        switch completions.count {
        case 0:
            // Нет вариантов - звук ошибки
            handleNoCompletions()
            
        case 1:
            // Один вариант - автоматически дополняем
            handleSingleCompletion(completions[0], originalText: originalText, cursorPosition: cursorPosition)
            
        default:
            // Несколько вариантов - ищем общий префикс или показываем список
            handleMultipleCompletions(completions, originalText: originalText, cursorPosition: cursorPosition)
        }
    }
    
    /// Обрабатывает случай когда нет вариантов автодополнения
    private func handleNoCompletions() {
        print("❌ Нет вариантов автодополнения")
        // Воспроизводим системный звук ошибки
        NSSound.beep()
        
        // Можно также показать временное сообщение
        showTemporaryMessage("Нет вариантов автодополнения")
    }
    
    /// Обрабатывает случай с одним вариантом автодополнения
    private func handleSingleCompletion(_ completion: String, originalText: String, cursorPosition: Int) {
        print("✅ Единственный вариант: '\(completion)'")
        
        // Применяем автодополнение к полю ввода
        applyCompletion(completion, originalText: originalText, cursorPosition: cursorPosition)
        
        showTemporaryMessage("Дополнено до: \(completion)")
    }
    
    /// Обрабатывает случай с несколькими вариантами автодополнения
    private func handleMultipleCompletions(_ completions: [String], originalText: String, cursorPosition: Int) {
        print("🔄 Несколько вариантов (\(completions.count))")
        
        // Ищем общий префикс среди всех вариантов
        let commonPrefix = findCommonPrefix(completions)
        let currentWord = extractCurrentWord(originalText, cursorPosition: cursorPosition)
        
        print("   Текущее слово: '\(currentWord)'")
        print("   Общий префикс: '\(commonPrefix)'")
        
        if commonPrefix.count > currentWord.count {
            // Есть общий префикс длиннее текущего слова - дополняем до него
            print("✅ Дополняем до общего префикса: '\(commonPrefix)'")
            applyCompletion(commonPrefix, originalText: originalText, cursorPosition: cursorPosition)
            showTemporaryMessage("Дополнено до общего префикса")
        } else {
            // Нет общего префикса - показываем список вариантов
            print("📋 Показываем список вариантов")
            showCompletionList(completions)
        }
    }
    
    /// Применяет автодополнение к полю ввода
    private func applyCompletion(_ completion: String, originalText: String, cursorPosition: Int) {
        // Извлекаем текущее слово для замены
        let currentWord = extractCurrentWord(originalText, cursorPosition: cursorPosition)
        let wordStart = findWordStart(originalText, cursorPosition: cursorPosition)
        
        // Создаем новый текст с заменой
        let beforeWord = String(originalText.prefix(wordStart))
        let afterCursor = String(originalText.suffix(originalText.count - cursorPosition))
        let newText = beforeWord + completion + afterCursor
        
        print("🔄 Применяем автодополнение:")
        print("   До слова: '\(beforeWord)'")
        print("   Заменяем: '\(currentWord)' → '\(completion)'")
        print("   После курсора: '\(afterCursor)'")
        print("   Результат: '\(newText)'")
        
        // Обновляем поле ввода
        commandTextField.stringValue = newText
        
        // Устанавливаем курсор в конец дополненного слова
        let newCursorPosition = beforeWord.count + completion.count
        setCursorPosition(newCursorPosition)
    }
    
    /// Извлекает текущее слово под курсором
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
    
    /// Находит начало текущего слова
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
    
    /// Находит общий префикс среди всех вариантов автодополнения
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
    
    /// Устанавливает позицию курсора в поле ввода
    private func setCursorPosition(_ position: Int) {
        if let fieldEditor = commandTextField.currentEditor() {
            let range = NSRange(location: position, length: 0)
            fieldEditor.selectedRange = range
        }
    }
    
    /// Показывает список вариантов автодополнения в терминале
    private func showCompletionList(_ completions: [String]) {
        let completionText = "💡 Варианты автодополнения:\n" + completions.map { "  \($0)" }.joined(separator: "\n") + "\n"
        appendOutput(completionText)
    }
    
    /// Показывает временное сообщение в статус баре
    private func showTemporaryMessage(_ message: String) {
        let originalStatus = statusLabel.stringValue
        statusLabel.stringValue = message
        
        // Возвращаем оригинальный статус через 2 секунды
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            self.statusLabel.stringValue = originalStatus
        }
    }
}

// MARK: - Global Document rebuild
extension TerminalViewController {
    /// Полностью перестраивает глобальную карту сегментов, начиная с указанного индекса блока.
    /// Для простоты пока пересчитываем весь документ.
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
        blockItem.configure(command: block.command, output: block.output, isFinished: block.isFinished, exitCode: block.exitCode)
        // применяем подсветку для текущего выделения, если оно попадает в этот блок
        applySelectionHighlightIfNeeded(to: blockItem, at: indexPath.item)
        return blockItem
    }

    func collectionView(_ collectionView: NSCollectionView, layout collectionViewLayout: NSCollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> NSSize {
        let contentWidth = collectionView.bounds.width - (collectionLayout.sectionInset.left + collectionLayout.sectionInset.right)
        let block = commandBlocks[indexPath.item]
        let height = CommandBlockItem.estimatedHeight(command: block.command, output: block.output, width: contentWidth)
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
        // Cmd+C — копирование выделенного текста
        if event.modifierFlags.contains(.command), let chars = event.charactersIgnoringModifiers, chars.lowercased() == "c" {
            copySelectionToPasteboard()
            return
        }
        super.keyDown(with: event)
    }

    /// Конвертирует координату клика в глобальный индекс символа, если попадает в текст
    private func hitTestGlobalIndex(at pointInRoot: NSPoint) -> (blockIndex: Int, globalIndex: Int)? {
        // Пройдёмся по видимым item-ам
        let visible = collectionView.visibleItems()
        for case let item as CommandBlockItem in visible {
            guard let indexPath = collectionView.indexPath(for: item) else { continue }
            // Конвертируем точку в координаты item
            let pointInItem = item.view.convert(pointInRoot, from: view)
            if !item.view.bounds.contains(pointInItem) { continue }

            // Проверяем заголовок
            if let hIdx = item.headerCharacterIndex(at: pointInItem) {
                let global = mapLocalToGlobal(blockIndex: indexPath.item, kind: .header, localIndex: hIdx)
                return (indexPath.item, global)
            }
            // Проверяем тело
            if let bIdx = item.bodyCharacterIndex(at: pointInItem) {
                let global = mapLocalToGlobal(blockIndex: indexPath.item, kind: .output, localIndex: bIdx)
                return (indexPath.item, global)
            }
        }
        return nil
    }

    /// Преобразует локальный индекс символа внутри блока в глобальный индекс по документу
    private func mapLocalToGlobal(blockIndex: Int, kind: SegmentKind, localIndex: Int) -> Int {
        for seg in globalDocument.segments {
            if seg.blockIndex == blockIndex && seg.kind == kind {
                return seg.range.location + min(localIndex, seg.range.length)
            }
        }
        // если сегмент не найден — возвращаем конец документа
        return globalDocument.totalLength
    }

    /// Подсвечивает актуальный selection во всех видимых ячейках
    private func updateSelectionHighlight() {
        guard let sel = selectionRange else {
            // сбрасываем подсветку
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

    /// Возвращает локальный диапазон внутри указанного сегмента для глобального диапазона selection
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

    /// Применяет подсветку при конфигурации ячейки
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
                    result += ns.substring(with: local)
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
