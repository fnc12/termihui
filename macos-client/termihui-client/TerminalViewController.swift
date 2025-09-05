import Cocoa
import SnapKit

/// Основной экран терминала
class TerminalViewController: NSViewController {
    
    // MARK: - UI Components
    private let toolbarView = NSView()
    private let disconnectButton = NSButton(title: "Отключиться", target: nil, action: nil)
    private let statusLabel = NSTextField(labelWithString: "Подключен")
    
    private let terminalScrollView = NSScrollView()
    private var terminalTextView = NSTextView()
    
    private let inputContainerView = NSView()
    private let commandTextField = TabHandlingTextField()
    private let sendButton = NSButton(title: "Отправить", target: nil, action: nil)
    private var inputUnderlineView: NSView!
    
    // MARK: - Properties
    weak var delegate: TerminalViewControllerDelegate?
    weak var webSocketManager: WebSocketManager?
    private var serverAddress: String = ""
    private let ansiParser = ANSIParser()
    
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
        // Настраиваем только scroll view, text view создадим позже
        terminalScrollView.hasVerticalScroller = true
        terminalScrollView.hasHorizontalScroller = false
        terminalScrollView.autohidesScrollers = false
        terminalScrollView.backgroundColor = NSColor.red  // ВРЕМЕННО: красный фон для диагностики
        terminalScrollView.borderType = .lineBorder
        terminalScrollView.drawsBackground = true
        
        // Создаём временный text view (будет заменён в viewDidAppear)
        terminalTextView.isEditable = false
        terminalTextView.backgroundColor = NSColor.black
        terminalScrollView.documentView = terminalTextView
        
        print("🔧 TerminalScrollView настроен: фон чёрный, размер: \(terminalScrollView.frame)")
    }
    
    private func updateTextViewFrame() {
        // Обновляем frame существующего NSTextView под размер ScrollView
        guard let textView = terminalScrollView.documentView as? NSTextView else { return }
        
        let contentSize = terminalScrollView.contentSize
        let currentContentHeight = textView.textStorage?.length ?? 0 > 0 ? textView.frame.height : contentSize.height
        
        // Новый frame: ширина = ScrollView, высота = максимум из ScrollView и текущего контента
        let newFrame = NSRect(
            x: 0, 
            y: 0, 
            width: contentSize.width, 
            height: max(contentSize.height, currentContentHeight)
        )
        
        textView.frame = newFrame
        print("🔧 Обновили frame NSTextView: \(newFrame)")
    }
    
    private func recreateTextViewWithCorrectSize() {
        // Теперь у scroll view есть правильные размеры!
        let contentSize = terminalScrollView.contentSize
        print("🔧 Пересоздаём NSTextView с размером: \(contentSize)")
        
        // Создаём text container с правильными размерами
        let textContainer = NSTextContainer(containerSize: NSSize(width: contentSize.width, height: CGFloat.greatestFiniteMagnitude))
        textContainer.widthTracksTextView = true
        textContainer.heightTracksTextView = false
        
        print("🔧 TextContainer создан с размером: \(textContainer.containerSize)")
        print("🔧 ContentSize был: \(contentSize)")
        
        // Создаём layout manager
        let layoutManager = NSLayoutManager()
        layoutManager.addTextContainer(textContainer)
        
        // Создаём text storage
        let textStorage = NSTextStorage()
        textStorage.addLayoutManager(layoutManager)
        
        // СОЗДАЁМ NSTextView с правильным frame
        let textViewFrame = NSRect(x: 0, y: 0, width: contentSize.width, height: contentSize.height)
        let newTextView = NSTextView(frame: textViewFrame, textContainer: textContainer)
        
        print("🔧 NSTextView создан с frame: \(textViewFrame)")
        
        // Настраиваем новый text view
        newTextView.isEditable = false
        newTextView.isSelectable = true
        newTextView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        newTextView.backgroundColor = NSColor.gray.withAlphaComponent(0.3)  // ВРЕМЕННО: полупрозрачный серый
        newTextView.textColor = NSColor.yellow  // ВРЕМЕННО: жёлтый текст для контраста
        
        // Создаём пустой attributed text - терминал начинается чистым
        let attributes: [NSAttributedString.Key: Any] = [
            .font: NSFont.monospacedSystemFont(ofSize: 12, weight: .regular),
            .foregroundColor: NSColor.green,
            .backgroundColor: NSColor.black
        ]
        // ВРЕМЕННО: добавляем тестовый текст для диагностики
        let testText = "🔧 TERMINAL VIEW РАБОТАЕТ\nЖдём вывода команд...\n"
        let attributedText = NSAttributedString(string: testText, attributes: attributes)
        newTextView.textStorage?.setAttributedString(attributedText)
        
        // Настройки ресайза - позволяем NSTextView расти по содержимому
        newTextView.isVerticallyResizable = true
        newTextView.isHorizontallyResizable = false
        newTextView.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
        newTextView.minSize = NSSize(width: 0, height: 0)  // Минимальный размер 0 - пусть растёт по контенту
        
        // Заменяем старый text view на новый
        terminalTextView = newTextView
        
        // Устанавливаем как document view
        terminalScrollView.documentView = terminalTextView
        
        print("✅ NSTextView пересоздан с размером: \(newTextView.frame)")
        print("🔧 ScrollView размер: \(terminalScrollView.frame)")
        print("🔧 ScrollView contentSize: \(terminalScrollView.contentSize)")
        print("🔧 TextView minSize: \(newTextView.minSize)")
        print("🔧 TextView backgroundColor: \(newTextView.backgroundColor)")
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
        DispatchQueue.main.async {
            // Парсим ANSI-коды в новом выводе
            let styledSegments = self.ansiParser.parse(output)
            let newAttributedText = styledSegments.toAttributedString()
            
            // Получаем текущий attributed text
            let currentAttributedText = self.terminalTextView.textStorage ?? NSMutableAttributedString()
            
            // Добавляем новый стилизованный текст
            currentAttributedText.append(newAttributedText)
            
            // Обновляем textStorage напрямую для лучшей производительности
            self.terminalTextView.textStorage?.setAttributedString(currentAttributedText)
            
            // Автоматический скролл к концу
            let range = NSRange(location: self.terminalTextView.textStorage?.length ?? 0, length: 0)
            self.terminalTextView.scrollRangeToVisible(range)
            
            print("✅ Стилизованный текст добавлен: \(styledSegments.count) сегментов")
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
    // Пока просто фиксация событий, без изменения текста.
    func didStartCommandBlock() {
        print("🧱 Начат блок команды")
        // Здесь позже будет логика добавления новой ячейки в collection view
    }
    
    func didFinishCommandBlock(exitCode: Int) {
        print("🏁 Завершён блок команды (exit=\(exitCode))")
        // Здесь позже будет логика завершения соответствующей ячейки
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



// MARK: - Delegate Protocol
protocol TerminalViewControllerDelegate: AnyObject {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String)
    func terminalViewControllerDidRequestDisconnect(_ controller: TerminalViewController)
}
