import Cocoa

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

