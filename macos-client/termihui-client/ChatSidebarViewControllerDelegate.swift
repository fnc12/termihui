import Foundation

/// Delegate for chat sidebar actions
protocol ChatSidebarViewControllerDelegate: AnyObject {
    func chatSidebarViewController(_ controller: ChatSidebarViewController, didSendMessage message: String, withProviderId providerId: UInt64)
    func chatSidebarViewControllerDidRequestProviders(_ controller: ChatSidebarViewController)
    func chatSidebarViewControllerDidRequestAddProvider(_ controller: ChatSidebarViewController)
    func chatSidebarViewControllerDidRequestChatHistory(_ controller: ChatSidebarViewController, forSession sessionId: UInt64)
}
