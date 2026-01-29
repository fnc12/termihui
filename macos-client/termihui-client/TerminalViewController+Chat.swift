import Cocoa

// MARK: - ChatSidebarViewControllerDelegate
extension TerminalViewController: ChatSidebarViewControllerDelegate {
    func chatSidebarViewController(_ controller: ChatSidebarViewController, didSendMessage message: String, withProviderId providerId: UInt64) {
        print("💬 AI chat message (provider \(providerId)): \(message)")
        
        // Start showing assistant response placeholder
        controller.startAssistantMessage()
        
        // Send to server via WebSocket
        clientCore?.send([
            "type": "ai_chat",
            "session_id": cachedActiveSessionId,
            "provider_id": providerId,
            "message": message
        ])
    }
    
    func chatSidebarViewControllerDidRequestProviders(_ controller: ChatSidebarViewController) {
        print("📋 Requesting LLM providers list")
        clientCore?.send(["type": "list_llm_providers"])
    }
    
    func chatSidebarViewControllerDidRequestAddProvider(_ controller: ChatSidebarViewController) {
        print("➕ Opening add provider dialog")
        showAddProviderSheet()
    }
    
    func chatSidebarViewControllerDidRequestChatHistory(_ controller: ChatSidebarViewController, forSession sessionId: UInt64) {
        print("📜 Requesting chat history for session \(sessionId)")
        clientCore?.send([
            "type": "get_chat_history",
            "session_id": sessionId
        ])
    }
    
    // MARK: - Add Provider Sheet
    private func showAddProviderSheet() {
        let alert = NSAlert()
        alert.messageText = "Add LLM Provider"
        alert.informativeText = "Enter provider details:"
        alert.alertStyle = .informational
        
        // Create input fields
        let accessory = NSView(frame: NSRect(x: 0, y: 0, width: 300, height: 160))
        
        let nameLabel = NSTextField(labelWithString: "Name:")
        nameLabel.frame = NSRect(x: 0, y: 130, width: 60, height: 20)
        accessory.addSubview(nameLabel)
        
        let nameField = NSTextField(frame: NSRect(x: 65, y: 130, width: 230, height: 22))
        nameField.placeholderString = "My Provider"
        accessory.addSubview(nameField)
        
        let urlLabel = NSTextField(labelWithString: "URL:")
        urlLabel.frame = NSRect(x: 0, y: 100, width: 60, height: 20)
        accessory.addSubview(urlLabel)
        
        let urlField = NSTextField(frame: NSRect(x: 65, y: 100, width: 230, height: 22))
        urlField.placeholderString = "http://localhost:11434"
        accessory.addSubview(urlField)
        
        let modelLabel = NSTextField(labelWithString: "Model:")
        modelLabel.frame = NSRect(x: 0, y: 70, width: 60, height: 20)
        accessory.addSubview(modelLabel)
        
        let modelField = NSTextField(frame: NSRect(x: 65, y: 70, width: 230, height: 22))
        modelField.placeholderString = "llama3 (optional)"
        accessory.addSubview(modelField)
        
        let apiKeyLabel = NSTextField(labelWithString: "API Key:")
        apiKeyLabel.frame = NSRect(x: 0, y: 40, width: 60, height: 20)
        accessory.addSubview(apiKeyLabel)
        
        let apiKeyField = NSSecureTextField(frame: NSRect(x: 65, y: 40, width: 230, height: 22))
        apiKeyField.placeholderString = "sk-... (optional)"
        accessory.addSubview(apiKeyField)
        
        alert.accessoryView = accessory
        alert.addButton(withTitle: "Add")
        alert.addButton(withTitle: "Cancel")
        
        alert.beginSheetModal(for: view.window!) { response in
            if response == .alertFirstButtonReturn {
                let name = nameField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
                let url = urlField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
                let model = modelField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines)
                let apiKey = apiKeyField.stringValue
                
                guard !name.isEmpty, !url.isEmpty else {
                    print("⚠️ Name and URL are required")
                    return
                }
                
                self.clientCore?.send([
                    "type": "add_llm_provider",
                    "name": name,
                    "provider_type": "openai_compatible",
                    "url": url,
                    "model": model,
                    "api_key": apiKey
                ])
                
                // Refresh providers list after adding
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
                    self.chatSidebarViewController?.requestProviders()
                }
            }
        }
    }
}
