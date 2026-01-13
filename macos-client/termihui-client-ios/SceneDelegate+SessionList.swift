import UIKit

// MARK: - SessionListViewControllerDelegate

extension SceneDelegate: SessionListViewControllerDelegate {
    func sessionListViewControllerDidRequestDisconnect(_ controller: SessionListViewController) {
        isUserInitiatedDisconnect = true
        clientCore?.send(["type": "disconnectButtonClicked"])
    }
}
