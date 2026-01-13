import UIKit

// MARK: - ServerListViewControllerDelegate

extension SceneDelegate: ServerListViewControllerDelegate {
    func serverListViewController(_ controller: ServerListViewController, didSelectServer address: String) {
        // Save address and reset state
        currentServerAddress = address
        isConnected = false
        isUserInitiatedDisconnect = false
        
        // Create SessionListVC and navigate to it
        let sessionListVC = SessionListViewController()
        sessionListVC.serverAddress = address
        sessionListVC.delegate = self
        self.sessionListVC = sessionListVC
        
        navigationController?.pushViewController(sessionListVC, animated: true)
        
        // Connect to server
        clientCore?.send(["type": "connectButtonClicked", "address": address])
    }
}
