import UIKit

// MARK: - TerminalViewControllerDelegate

extension SceneDelegate: TerminalViewControllerDelegate {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String) {
        print("📤 Sending command: \(command)")
        clientCore?.send([
            "type": "executeCommand",
            "command": command
        ])
    }
    
    func terminalViewControllerDidClose(_ controller: TerminalViewController) {
        print("📱 Terminal closed")
        terminalVC = nil
        _chatVC = nil
    }
    
    func terminalViewControllerDidRequestChat(_ controller: TerminalViewController) {
        print("💬 Opening AI Chat")
        
        let chatVC = ChatViewController()
        chatVC.sessionId = controller.sessionId
        chatVC.delegate = self
        chatVC.title = controller.title
        _chatVC = chatVC
        
        navigationController?.pushViewController(chatVC, animated: true)
    }
}
