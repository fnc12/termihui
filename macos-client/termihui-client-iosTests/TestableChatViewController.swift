import UIKit
@testable import termihui_client_ios

final class TestableChatViewController: ChatViewController {
    enum Call: Equatable {
        case addUserMessage(String)
    }
    
    var calls: [Call] = []
    
    override func addUserMessage(_ text: String) {
        calls.append(.addUserMessage(text))
    }
}
