import Cocoa

/// Custom NSTextField with Tab interception support for autocomplete
class TabHandlingTextField: NSTextField {
    
    weak var tabDelegate: TabHandlingTextFieldDelegate?
    
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
        // Create custom field editor to intercept Tab
        print("🔧 TabHandlingTextField initialized")
    }
    
    // Override textShouldBeginEditing to set field editor delegate
    override func textShouldBeginEditing(_ textObject: NSText) -> Bool {
        print("🔧 textShouldBeginEditing called")
        
        // Set ourselves as delegate for field editor
        if let textView = textObject as? NSTextView {
            textView.delegate = self
            print("🔧 Field editor delegate set")
        }
        
        return super.textShouldBeginEditing(textObject)
    }
}

// MARK: - NSTextViewDelegate for key interception in field editor
extension TabHandlingTextField: NSTextViewDelegate {
    
    func textView(_ textView: NSTextView, doCommandBy commandSelector: Selector) -> Bool {
        print("🔧 doCommandBy called with selector: \(commandSelector)")
        
        // Check for Tab command
        if commandSelector == #selector(NSResponder.insertTab(_:)) {
            print("🔧 Tab intercepted via field editor!")
            print("🔧 Current text: '\(self.stringValue)'")
            print("🔧 Cursor position: \(textView.selectedRange().location)")
            
            // Notify delegate about Tab press
            tabDelegate?.tabHandlingTextField(self, didPressTabWithText: self.stringValue, cursorPosition: textView.selectedRange().location)
            
            return true // Handle event ourselves, don't pass further
        }
        
        // For all other commands - standard behavior
        return false
    }
}

protocol TabHandlingTextFieldDelegate: AnyObject {
    func tabHandlingTextField(_ textField: TabHandlingTextField, didPressTabWithText text: String, cursorPosition: Int)
}
