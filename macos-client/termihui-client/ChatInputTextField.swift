import Cocoa

/// Custom NSTextField for chat input with Shift+Enter support for new lines
class ChatInputTextField: NSTextField {
    
    /// Callback for sending message (Enter without Shift)
    var onSendMessage: (() -> Void)?
    
    /// Callback for height changes
    var onHeightChanged: ((CGFloat) -> Void)?
    
    /// Minimum height for the field
    private let minHeight: CGFloat = 24
    
    /// Maximum height (to prevent infinite growth)
    private let maxHeight: CGFloat = 120
    
    /// Current calculated height
    private var currentHeight: CGFloat = 24
    
    override func awakeFromNib() {
        super.awakeFromNib()
        setupTextField()
    }
    
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupTextField()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupTextField()
    }
    
    private func setupTextField() {
        // Enable word wrapping for multi-line support
        self.cell?.wraps = true
        self.cell?.isScrollable = false
        self.lineBreakMode = .byWordWrapping
        self.usesSingleLineMode = false
    }
    
    deinit {
        NotificationCenter.default.removeObserver(self)
    }
    
    @objc private func handleFieldEditorTextDidChange(_ notification: Notification) {
        updateHeightIfNeeded()
    }
    
    override var intrinsicContentSize: NSSize {
        var size = super.intrinsicContentSize
        
        // Calculate required height based on text content
        let calculatedHeight = calculateRequiredHeight()
        size.height = max(minHeight, min(calculatedHeight, maxHeight))
        
        return size
    }
    
    /// Calculates the required height for the current text
    private func calculateRequiredHeight() -> CGFloat {
        guard bounds.width > 0 else {
            return minHeight
        }
        
        // Get current text - from field editor if editing, otherwise from stringValue
        let currentText: String
        if let fieldEditor = self.currentEditor() as? NSTextView {
            currentText = fieldEditor.string
        } else {
            currentText = self.stringValue
        }
        
        guard !currentText.isEmpty else {
            return minHeight
        }
        
        // Calculate height using text bounding rect
        let font = self.font ?? NSFont.systemFont(ofSize: NSFont.systemFontSize)
        let textWidth = bounds.width - 4 // Small padding
        
        let textStorage = NSTextStorage(string: currentText)
        textStorage.addAttribute(.font, value: font, range: NSRange(location: 0, length: textStorage.length))
        
        let layoutManager = NSLayoutManager()
        textStorage.addLayoutManager(layoutManager)
        
        let textContainer = NSTextContainer(size: NSSize(width: textWidth, height: CGFloat.greatestFiniteMagnitude))
        textContainer.lineFragmentPadding = 0
        layoutManager.addTextContainer(textContainer)
        
        layoutManager.ensureLayout(for: textContainer)
        let usedRect = layoutManager.usedRect(for: textContainer)
        
        return ceil(usedRect.height) + 6 // Add some vertical padding
    }
    
    /// Call this when text changes to update height
    func updateHeightIfNeeded() {
        let newHeight = max(minHeight, min(calculateRequiredHeight(), maxHeight))
        
        if abs(newHeight - currentHeight) > 1 {
            currentHeight = newHeight
            invalidateIntrinsicContentSize()
            onHeightChanged?(newHeight)
        }
    }
    
    // Override textShouldBeginEditing to set field editor delegate
    override func textShouldBeginEditing(_ textObject: NSText) -> Bool {
        // Set ourselves as delegate for field editor
        if let textView = textObject as? NSTextView {
            textView.delegate = self
            
            // Subscribe to field editor's text changes
            NotificationCenter.default.removeObserver(self, name: NSText.didChangeNotification, object: nil)
            NotificationCenter.default.addObserver(
                self,
                selector: #selector(handleFieldEditorTextDidChange(_:)),
                name: NSText.didChangeNotification,
                object: textView
            )
        }
        
        return super.textShouldBeginEditing(textObject)
    }
}

// MARK: - NSTextViewDelegate for key interception in field editor
extension ChatInputTextField: NSTextViewDelegate {
    
    func textView(_ textView: NSTextView, doCommandBy commandSelector: Selector) -> Bool {
        // Intercept Enter/Return key
        if commandSelector == #selector(NSResponder.insertNewline(_:)) {
            let modifierFlags = NSEvent.modifierFlags
            // Check if Shift or Control is pressed
            let shiftPressed = modifierFlags.contains(.shift)
            let controlPressed = modifierFlags.contains(.control)
            
            if shiftPressed || controlPressed {
                // Shift+Enter or Ctrl+Enter: insert new line
                textView.insertNewlineIgnoringFieldEditor(nil)
                updateHeightIfNeeded()
                return true
            } else {
                // Enter without modifiers: send message
                onSendMessage?()
                return true
            }
        }
        return false
    }
}
