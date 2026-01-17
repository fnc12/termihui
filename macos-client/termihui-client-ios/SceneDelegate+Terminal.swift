import UIKit

// MARK: - TerminalViewControllerDelegate

extension SceneDelegate: TerminalViewControllerDelegate {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String) {
        print("📤 Sending command: \(command)")
        clientCore?.send([
            "type": "execute",
            "command": command
        ])
    }
    
    func terminalViewControllerDidClose(_ controller: TerminalViewController) {
        print("📱 Terminal closed")
        terminalVC = nil
    }
}
