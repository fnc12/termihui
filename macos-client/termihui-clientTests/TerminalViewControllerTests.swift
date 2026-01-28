import XCTest
@testable import termihui_client_macos

final class TerminalViewControllerTests: XCTestCase {
    
    var sut: TerminalViewController!
    var mockSessionListController: MockSessionListViewController!
    var mockChatSidebarController: MockChatSidebarViewController!
    
    override func setUpWithError() throws {
        sut = TerminalViewController()
        mockSessionListController = MockSessionListViewController()
        mockChatSidebarController = MockChatSidebarViewController()
        sut.sessionListController = mockSessionListController
        sut.chatSidebarController = mockChatSidebarController
    }
    
    override func tearDownWithError() throws {
        sut = nil
        mockSessionListController = nil
        mockChatSidebarController = nil
    }
    
    // MARK: - setActiveSession Tests
    
    func testSetActiveSession() throws {
        // Given
        let expectedSessionId: UInt64 = 42
        XCTAssertNotEqual(sut.cachedActiveSessionId, expectedSessionId)
        XCTAssertTrue(mockSessionListController.calls.isEmpty)
        XCTAssertTrue(mockChatSidebarController.calls.isEmpty)
        XCTAssertNotEqual(mockChatSidebarController.sessionId, expectedSessionId)
        
        // When
        sut.setActiveSession(expectedSessionId)
        
        // Then
        XCTAssertEqual(sut.cachedActiveSessionId, expectedSessionId)
        XCTAssertEqual(mockSessionListController.calls, [.setActiveSession(expectedSessionId)])
        XCTAssertEqual(mockChatSidebarController.sessionId, expectedSessionId)
        XCTAssertEqual(mockChatSidebarController.calls, [.clearMessages])
    }
}
