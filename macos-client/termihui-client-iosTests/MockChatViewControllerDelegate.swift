import UIKit
@testable import termihui_client_ios

final class MockChatViewControllerDelegate: ChatViewControllerDelegate {
    enum Call: Equatable {
        case didSendMessage(message: String, providerId: UInt64)
        case didRequestProviders
        case didRequestManageProviders
        case didRequestChatHistory(sessionId: UInt64)
        case didClose
    }
    
    private(set) var calls: [Call] = []
    
    func chatViewController(_ controller: ChatViewController, didSendMessage message: String, withProviderId providerId: UInt64) {
        calls.append(.didSendMessage(message: message, providerId: providerId))
    }
    
    func chatViewControllerDidRequestProviders(_ controller: ChatViewController) {
        calls.append(.didRequestProviders)
    }
    
    func chatViewControllerDidRequestManageProviders(_ controller: ChatViewController) {
        calls.append(.didRequestManageProviders)
    }
    
    func chatViewControllerDidRequestChatHistory(_ controller: ChatViewController, forSession sessionId: UInt64) {
        calls.append(.didRequestChatHistory(sessionId: sessionId))
    }
    
    func chatViewControllerDidClose(_ controller: ChatViewController) {
        calls.append(.didClose)
    }
}
