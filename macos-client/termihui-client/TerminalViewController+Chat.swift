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
    
    func chatSidebarViewControllerDidRequestManageProviders(_ controller: ChatSidebarViewController) {
        print("⚙️ Opening LLM providers management")
        showProvidersSheet()
    }
    
    func chatSidebarViewControllerDidRequestChatHistory(_ controller: ChatSidebarViewController, forSession sessionId: UInt64) {
        print("📜 Requesting chat history for session \(sessionId)")
        clientCore?.send([
            "type": "get_chat_history",
            "session_id": sessionId
        ])
    }
    
    // MARK: - Providers Sheet
    
    private func showProvidersSheet() {
        let providersVC = LLMProvidersViewController(providers: cachedProviders)
        providersVC.delegate = self
        self.llmProvidersVC = providersVC
        presentAsSheet(providersVC)
    }
}
