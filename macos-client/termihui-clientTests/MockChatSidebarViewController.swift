import Cocoa
@testable import termihui_client_macos

final class MockChatSidebarViewController: NSViewController, ChatSidebarViewController {
    enum Call: Equatable {
        case setInteractive(Bool)
        case focusInputField
        case startAssistantMessage
        case appendChunk(String)
        case finishAssistantMessage
        case showError(String)
        case updateProviders([LLMProvider])
        case clearMessages
        case requestProviders
        case requestChatHistory
        case loadChatHistory(Int)  // message count
        case setLoading(Bool)
    }
    
    private(set) var calls: [Call] = []
    var sessionId: UInt64 = 0
    
    func setInteractive(_ interactive: Bool) {
        calls.append(.setInteractive(interactive))
    }
    
    func focusInputField() {
        calls.append(.focusInputField)
    }
    
    func startAssistantMessage() {
        calls.append(.startAssistantMessage)
    }
    
    func appendChunk(_ text: String) {
        calls.append(.appendChunk(text))
    }
    
    func finishAssistantMessage() {
        calls.append(.finishAssistantMessage)
    }
    
    func showError(_ error: String) {
        calls.append(.showError(error))
    }
    
    func updateProviders(_ newProviders: [LLMProvider]) {
        calls.append(.updateProviders(newProviders))
    }
    
    func clearMessages() {
        calls.append(.clearMessages)
    }
    
    func requestProviders() {
        calls.append(.requestProviders)
    }
    
    func requestChatHistory() {
        calls.append(.requestChatHistory)
    }
    
    func loadChatHistory(_ messages: [ChatMessageInfo]) {
        calls.append(.loadChatHistory(messages.count))
    }
    
    func setLoading(_ loading: Bool) {
        calls.append(.setLoading(loading))
    }
}
