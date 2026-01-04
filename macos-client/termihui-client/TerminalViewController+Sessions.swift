import Cocoa

// MARK: - SessionListViewControllerDelegate
extension TerminalViewController: SessionListViewControllerDelegate {
    func sessionListViewControllerDidRequestNewSession(_ controller: SessionListViewController) {
        print("📱 Request new session")
        clientCore?.send(["type": "createSession"])
    }
    
    func sessionListViewController(_ controller: SessionListViewController, didSelectSession sessionId: UInt64) {
        print("📱 Select session: #\(sessionId)")
        // Update client-core's activeSessionId (local only, not sent to server)
        clientCore?.send(["type": "switchSession", "sessionId": sessionId])
        setActiveSession(sessionId)
        clearState()
        updateSessionName(sessionId)
        toggleSidebar()
    }
    
    func sessionListViewController(_ controller: SessionListViewController, didRequestDeleteSession sessionId: UInt64) {
        print("📱 Delete session: #\(sessionId)")
        clientCore?.send(["type": "closeSession", "sessionId": sessionId])
    }
}

// MARK: - Session Management (Public API)
extension TerminalViewController {
    /// Updates the session list from server data
    func updateSessionList(_ sessions: [SessionInfo], activeSessionId: UInt64?) {
        // Cache for lazy sidebar creation
        cachedSessions = sessions
        cachedActiveSessionId = activeSessionId ?? 0
        
        // Update sidebar if it exists
        sessionListController?.updateSessions(sessions, activeId: cachedActiveSessionId)
    }
    
    /// Updates active session highlight
    func setActiveSession(_ sessionId: UInt64) {
        cachedActiveSessionId = sessionId
        sessionListController?.setActiveSession(sessionId)
    }
}

