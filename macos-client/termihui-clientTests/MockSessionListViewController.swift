import Cocoa
@testable import termihui_client_macos

final class MockSessionListViewController: NSViewController, SessionListViewController {
    enum Call: Equatable {
        case updateSessions(sessions: [SessionInfo], activeId: UInt64)
        case setActiveSession(UInt64)
        case setInteractive(Bool)
    }
    
    private(set) var calls: [Call] = []
    
    func updateSessions(_ sessions: [SessionInfo], activeId: UInt64) {
        calls.append(.updateSessions(sessions: sessions, activeId: activeId))
    }
    
    func setActiveSession(_ sessionId: UInt64) {
        calls.append(.setActiveSession(sessionId))
    }
    
    func setInteractive(_ interactive: Bool) {
        calls.append(.setInteractive(interactive))
    }
}
