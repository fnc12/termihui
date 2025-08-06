//
//  TerminalViewController.swift
//  termihui-client
//
//  Created by TermiHUI on 05.08.2025.
//

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
    private let commandTextField = NSTextField()
    private let sendButton = NSButton(title: "Отправить", target: nil, action: nil)
    
    // MARK: - Properties
    weak var delegate: TerminalViewControllerDelegate?
    private var serverAddress: String = ""
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupLayout()
        setupActions()
    }
    
    override func viewDidAppear() {
        super.viewDidAppear()
        
        // ПЕРЕСОЗДАЁМ NSTextView ПОСЛЕ того как layout завершён
        recreateTextViewWithCorrectSize()
        
        // Устанавливаем фокус на поле ввода команд
        view.window?.makeFirstResponder(commandTextField)
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
        terminalScrollView.backgroundColor = NSColor.black
        terminalScrollView.borderType = .noBorder
        
        // Создаём временный text view (будет заменён в viewDidAppear)
        terminalTextView.isEditable = false
        terminalTextView.backgroundColor = NSColor.black
        terminalScrollView.documentView = terminalTextView
    }
    
    private func recreateTextViewWithCorrectSize() {
        // Теперь у scroll view есть правильные размеры!
        let contentSize = terminalScrollView.contentSize
        print("🔧 Пересоздаём NSTextView с размером: \(contentSize)")
        
        // Создаём text container с правильными размерами
        let textContainer = NSTextContainer(containerSize: NSSize(width: contentSize.width, height: CGFloat.greatestFiniteMagnitude))
        textContainer.widthTracksTextView = true
        textContainer.heightTracksTextView = false
        
        // Создаём layout manager
        let layoutManager = NSLayoutManager()
        layoutManager.addTextContainer(textContainer)
        
        // Создаём text storage
        let textStorage = NSTextStorage()
        textStorage.addLayoutManager(layoutManager)
        
        // СОЗДАЁМ NSTextView с правильными размерами
        let newTextView = NSTextView(frame: NSRect(x: 0, y: 0, width: contentSize.width, height: contentSize.height), textContainer: textContainer)
        
        // Настраиваем новый text view
        newTextView.isEditable = false
        newTextView.isSelectable = true
        newTextView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        newTextView.backgroundColor = NSColor.black
        newTextView.textColor = NSColor.green
        newTextView.string = "TermiHUI Terminal v1.0.0\nГотов к работе...\n\n"
        
        // Настройки ресайза
        newTextView.isVerticallyResizable = true
        newTextView.isHorizontallyResizable = false
        newTextView.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
        newTextView.minSize = NSSize(width: 0, height: contentSize.height)
        
        // Заменяем старый text view на новый
        terminalTextView = newTextView
        
        // Устанавливаем как document view
        terminalScrollView.documentView = terminalTextView
        
        print("✅ NSTextView пересоздан с размером: \(newTextView.frame)")
    }
    
    private func setupInputView() {
        inputContainerView.wantsLayer = true
        inputContainerView.layer?.backgroundColor = NSColor.separatorColor.cgColor
        
        // Command text field
        commandTextField.placeholderString = "Введите команду..."
        commandTextField.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        commandTextField.target = self
        commandTextField.action = #selector(sendCommand)
        
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
        
        // Terminal view
        terminalScrollView.snp.makeConstraints { make in
            make.top.equalTo(toolbarView.snp.bottom)
            make.leading.trailing.equalToSuperview()
            make.bottom.equalTo(inputContainerView.snp.top)
        }
        
        // Input container
        inputContainerView.snp.makeConstraints { make in
            make.leading.trailing.bottom.equalToSuperview()
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
        print("📺 TerminalViewController.appendOutput вызван с: \(output)")
        DispatchQueue.main.async {
            let currentText = self.terminalTextView.string
            let newText = currentText + output
            
            print("📝 Обновляем текст с '\(currentText)' на '\(newText)'")
            print("🎨 Цвет текста: \(self.terminalTextView.textColor?.description ?? "nil")")
            print("🎨 Цвет фона: \(self.terminalTextView.backgroundColor.description)")
            print("📏 Размер шрифта: \(self.terminalTextView.font?.pointSize ?? 0)")
            print("📐 Размер view: \(self.terminalTextView.frame)")
            print("📊 Длина текста: \(newText.count) символов")
            
            self.terminalTextView.string = newText
            
            // ПРИНУДИТЕЛЬНОЕ ОБНОВЛЕНИЕ
            self.terminalTextView.needsDisplay = true
            self.terminalTextView.needsLayout = true
            
            // Автоматический скролл к концу
            let range = NSRange(location: self.terminalTextView.string.count, length: 0)
            self.terminalTextView.scrollRangeToVisible(range)
            print("✅ Текст обновлен и принудительно перерисован")
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
        
        // Добавляем команду в терминал (как эхо)
        appendOutput("$ \(command)\n")
        
        // Отправляем команду через delegate
        delegate?.terminalViewController(self, didSendCommand: command)
    }
    
    @objc private func disconnectButtonTapped() {
        delegate?.terminalViewControllerDidRequestDisconnect(self)
    }
}

// MARK: - Delegate Protocol
protocol TerminalViewControllerDelegate: AnyObject {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String)
    func terminalViewControllerDidRequestDisconnect(_ controller: TerminalViewController)
}