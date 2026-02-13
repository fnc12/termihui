import Foundation

/// Delegate for LLM providers management actions (add, update, delete)
protocol LLMProvidersViewControllerDelegate: AnyObject {
    func llmProvidersDidRequestAdd(name: String, url: String, model: String, apiKey: String)
    func llmProvidersDidRequestUpdate(id: UInt64, name: String, url: String, model: String, apiKey: String)
    func llmProvidersDidRequestDelete(id: UInt64)
    func llmProvidersDidDismiss()
}
