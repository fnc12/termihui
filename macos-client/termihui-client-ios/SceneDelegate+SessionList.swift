import UIKit

// MARK: - SessionListViewControllerDelegate

extension SceneDelegate: SessionListViewControllerDelegate {
    func sessionListViewControllerDidRequestDisconnect(_ controller: SessionListViewController) {
        isUserInitiatedDisconnect = true
        clientCore?.send(["type": "disconnectButtonClicked"])
    }
    
    func sessionListViewController(_ controller: SessionListViewController, didSelectSession sessionId: UInt64) {
        print("📱 Selected session: #\(sessionId)")
        
        // Switch session in client-core
        clientCore?.send(["type": "switchSession", "sessionId": sessionId])
        
        // Create and show terminal view controller
        let terminalVC = TerminalViewController()
        terminalVC.sessionId = sessionId
        terminalVC.delegate = self
        self.terminalVC = terminalVC
        
        navigationController?.pushViewController(terminalVC, animated: true)
    }
}
