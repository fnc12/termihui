import UIKit

// MARK: - Chat View Controller Reference

extension SceneDelegate {
    /// Current chat view controller
    var chatVC: ChatViewController? {
        return _chatVC
    }
}

// MARK: - ChatViewControllerDelegate

extension SceneDelegate: ChatViewControllerDelegate {
    
    func chatViewController(_ controller: ChatViewController, didSendMessage message: String, withProviderId providerId: UInt64) {
        guard let sessionId = terminalVC?.sessionId else {
            print("❌ No active session for AI chat")
            return
        }
        
        print("💬 AI chat message (provider \(providerId)): \(message)")
        
        // Start showing assistant response placeholder immediately
        controller.startAssistantMessage()
        
        // Send to server
        clientCore?.send([
            "type": "ai_chat",
            "session_id": sessionId,
            "provider_id": providerId,
            "message": message
        ])
    }
    
    func chatViewControllerDidRequestProviders(_ controller: ChatViewController) {
        print("📤 Requesting LLM providers")
        clientCore?.send(["type": "list_llm_providers"])
    }
    
    func chatViewControllerDidRequestManageProviders(_ controller: ChatViewController) {
        print("⚙️ Opening LLM providers management")
        let providersVC = LLMProvidersViewController(providers: controller.currentProviders)
        providersVC.delegate = self
        self._llmProvidersVC = providersVC
        navigationController?.pushViewController(providersVC, animated: true)
    }
    
    func chatViewControllerDidRequestChatHistory(_ controller: ChatViewController, forSession sessionId: UInt64) {
        clientCore?.send([
            "type": "get_chat_history",
            "session_id": sessionId
        ])
    }
    
    func chatViewControllerDidClose(_ controller: ChatViewController) {
        print("📱 ChatViewController closed")
    }
}

// MARK: - AI Event Handling

extension SceneDelegate {
    
    /// Handle AI-related server messages
    func handleAIMessage(_ messageType: String, _ messageDict: [String: Any]) -> Bool {
        switch messageType {
        case "ai_chunk":
            if let content = messageDict["content"] as? String,
               let sessionId = messageDict["session_id"] as? UInt64 {
                if chatVC?.sessionId == sessionId {
                    chatVC?.appendChunk(content)
                }
            }
            return true
            
        case "ai_done":
            if let sessionId = messageDict["session_id"] as? UInt64 {
                if chatVC?.sessionId == sessionId {
                    chatVC?.finishAssistantMessage()
                }
            }
            return true
            
        case "ai_error":
            if let error = messageDict["error"] as? String,
               let sessionId = messageDict["session_id"] as? UInt64 {
                if chatVC?.sessionId == sessionId {
                    chatVC?.showError(error)
                }
            }
            return true
            
        case "chat_history":
            guard let sessionId = messageDict["session_id"] as? UInt64 else {
                return true
            }
            guard let messagesData = messageDict["messages"] else {
                return true
            }
            
            do {
                let jsonData = try JSONSerialization.data(withJSONObject: messagesData)
                let messages = try JSONDecoder().decode([ChatMessageInfo].self, from: jsonData)
                if chatVC?.sessionId == sessionId {
                    chatVC?.loadChatHistory(messages)
                }
            } catch {
                print("❌ [chat_history] decode error: \(error)")
            }
            return true
            
        case "llm_providers_list":
            if let providersData = messageDict["providers"] {
                do {
                    let jsonData = try JSONSerialization.data(withJSONObject: providersData)
                    let providers = try JSONDecoder().decode([LLMProvider].self, from: jsonData)
                    chatVC?.updateProviders(providers)
                    _llmProvidersVC?.updateProviders(providers)
                } catch {
                    print("❌ [llm_providers_list] decode error: \(error)")
                }
            }
            return true
            
        default:
            return false
        }
    }
}
