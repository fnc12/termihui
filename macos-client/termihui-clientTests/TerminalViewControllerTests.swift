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
    
    // MARK: - Layout Tests
    
    /// Test that terminalScrollView does not overlap with inputContainerView
    /// Bug: Command history list was going under the input bar
    func testTerminalScrollViewDoesNotOverlapInputContainer() throws {
        // Given: Create a window to host the view controller (required for layout)
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 800, height: 600),
            styleMask: [.titled, .resizable],
            backing: .buffered,
            defer: false
        )
        window.contentViewController = sut
        
        // When: Force layout
        sut.view.layoutSubtreeIfNeeded()
        
        // Then: terminalScrollView should end exactly where inputContainerView begins
        // In flipped coordinates (AppKit uses bottom-left origin):
        // terminalScrollView.frame.minY should equal inputContainerView.frame.maxY
        // But with SnapKit constraints, we check that they don't overlap
        
        let terminalFrame = sut.terminalScrollView.frame
        let inputFrame = sut.inputContainerView.frame
        
        // Terminal scroll view bottom edge (minY) should be >= input container top edge (maxY)
        // This ensures no overlap
        XCTAssertGreaterThanOrEqual(
            terminalFrame.minY,
            inputFrame.maxY,
            "terminalScrollView (minY: \(terminalFrame.minY)) should not overlap with inputContainerView (maxY: \(inputFrame.maxY))"
        )
        
        // Additionally verify that inputContainerView has positive height (is visible)
        XCTAssertGreaterThan(
            inputFrame.height,
            0,
            "inputContainerView should have positive height"
        )
        
        // Verify terminalScrollView has reasonable height
        XCTAssertGreaterThanOrEqual(
            terminalFrame.height,
            200,
            "terminalScrollView should have at least minimum height of 200"
        )
    }
    
    /// Test that layout is correct after entering and exiting raw input mode
    func testLayoutAfterRawModeToggle() throws {
        // Given: Create a window to host the view controller
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 800, height: 600),
            styleMask: [.titled, .resizable],
            backing: .buffered,
            defer: false
        )
        window.contentViewController = sut
        sut.view.layoutSubtreeIfNeeded()
        
        // When: Enter and exit raw mode
        sut.enterRawInputMode()
        sut.view.layoutSubtreeIfNeeded()
        sut.exitRawInputMode()
        sut.view.layoutSubtreeIfNeeded()
        
        // Then: Layout should be restored correctly
        let terminalFrame = sut.terminalScrollView.frame
        let inputFrame = sut.inputContainerView.frame
        
        XCTAssertGreaterThanOrEqual(
            terminalFrame.minY,
            inputFrame.maxY,
            "After exiting raw mode, terminalScrollView should not overlap with inputContainerView"
        )
        
        XCTAssertFalse(
            sut.inputContainerView.isHidden,
            "inputContainerView should be visible after exiting raw mode"
        )
    }
}
