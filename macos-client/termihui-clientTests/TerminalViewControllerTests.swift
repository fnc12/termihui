//
//  TerminalViewControllerTests.swift
//  termihui-clientTests
//

import XCTest
@testable import termihui_client_macos

final class TerminalViewControllerTests: XCTestCase {
    
    var sut: TerminalViewController!
    
    override func setUpWithError() throws {
        sut = TerminalViewController()
    }
    
    override func tearDownWithError() throws {
        sut = nil
    }
    
    // MARK: - setActiveSession Tests
    
    func testSetActiveSession_updatesCachedActiveSessionId() throws {
        // Given
        let expectedSessionId: UInt64 = 42
        XCTAssertNotEqual(sut.cachedActiveSessionId, expectedSessionId)
        
        // When
        sut.setActiveSession(expectedSessionId)
        
        // Then
        XCTAssertEqual(sut.cachedActiveSessionId, expectedSessionId)
    }
}
