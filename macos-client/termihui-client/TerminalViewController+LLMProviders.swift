import Cocoa

// MARK: - LLMProvidersViewControllerDelegate

extension TerminalViewController: LLMProvidersViewControllerDelegate {
    func llmProvidersDidRequestAdd(name: String, url: String, model: String, apiKey: String) {
        print("➕ Adding LLM provider: \(name)")
        clientCore?.send([
            "type": "add_llm_provider",
            "name": name,
            "provider_type": "openai_compatible",
            "url": url,
            "model": model,
            "api_key": apiKey
        ])
        
        refreshProvidersList()
    }
    
    func llmProvidersDidRequestUpdate(id: UInt64, name: String, url: String, model: String, apiKey: String) {
        print("✏️ Updating LLM provider \(id): \(name)")
        clientCore?.send([
            "type": "update_llm_provider",
            "id": id,
            "name": name,
            "url": url,
            "model": model,
            "api_key": apiKey
        ])
        
        refreshProvidersList()
    }
    
    func llmProvidersDidRequestDelete(id: UInt64) {
        print("🗑️ Deleting LLM provider \(id)")
        clientCore?.send([
            "type": "delete_llm_provider",
            "id": id
        ])
        
        refreshProvidersList()
    }
    
    func llmProvidersDidDismiss() {
        print("✅ Providers management dismissed")
        llmProvidersVC = nil
    }
    
    private func refreshProvidersList() {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [weak self] in
            self?.clientCore?.send(["type": "list_llm_providers"])
        }
    }
}
