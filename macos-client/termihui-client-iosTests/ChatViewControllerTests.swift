import XCTest
@testable import termihui_client_ios

final class ChatViewControllerTests: XCTestCase {
    
    var sut: TestableChatViewController!
    var mockDelegate: MockChatViewControllerDelegate!
    
    override func setUpWithError() throws {
        sut = TestableChatViewController()
        mockDelegate = MockChatViewControllerDelegate()
        sut.delegate = mockDelegate
    }
    
    override func tearDownWithError() throws {
        sut = nil
        mockDelegate = nil
    }
    
    // MARK: - sendCurrentMessage Tests
    
    func testSendCurrentMessage() {
        struct TestCase: CustomStringConvertible {
            let description: String
            let text: String?
            let selectedProviderId: UInt64
            let expectedResult: ChatViewController.SendCurrentMessageResult
            let expectedCalls: [TestableChatViewController.Call]
            let expectedDelegateCalls: [MockChatViewControllerDelegate.Call]
            let expectedInputTextAfter: String?
        }
        
        let testCases: [TestCase] = [
            // 1) Empty / nil / whitespace text
            TestCase(
                description: "empty text returns .textIsEmpty",
                text: "",
                selectedProviderId: 42,
                expectedResult: .textIsEmpty,
                expectedCalls: [],
                expectedDelegateCalls: [],
                expectedInputTextAfter: ""
            ),
            TestCase(
                description: "whitespace-only text returns .textIsEmpty",
                text: "   ",
                selectedProviderId: 42,
                expectedResult: .textIsEmpty,
                expectedCalls: [],
                expectedDelegateCalls: [],
                expectedInputTextAfter: "   "
            ),
            // 2) Text exists but no provider selected
            TestCase(
                description: "valid text but providerId == 0 returns .noProviderError",
                text: "Hello AI",
                selectedProviderId: 0,
                expectedResult: .noProviderError,
                expectedCalls: [],
                expectedDelegateCalls: [],
                expectedInputTextAfter: "Hello AI"
            ),
            // 3) Valid text and valid provider
            TestCase(
                description: "valid text and provider returns .ok, calls addUserMessage and delegate",
                text: "Hello AI",
                selectedProviderId: 42,
                expectedResult: .ok,
                expectedCalls: [.addUserMessage("Hello AI")],
                expectedDelegateCalls: [.didSendMessage(message: "Hello AI", providerId: 42)],
                expectedInputTextAfter: ""
            ),
        ]
        
        for tc in testCases {
            // Reset state
            sut.calls = []
            mockDelegate = MockChatViewControllerDelegate()
            sut.delegate = mockDelegate
            sut.inputTextField.text = tc.text
            sut.selectedProviderId = tc.selectedProviderId
            
            // When
            let result = sut.sendCurrentMessage()
            
            // Then
            XCTAssertEqual(result, tc.expectedResult, "[\(tc.description)] unexpected result")
            XCTAssertEqual(sut.calls, tc.expectedCalls, "[\(tc.description)] unexpected calls")
            XCTAssertEqual(mockDelegate.calls, tc.expectedDelegateCalls, "[\(tc.description)] unexpected delegate calls")
            XCTAssertEqual(sut.inputTextField.text, tc.expectedInputTextAfter, "[\(tc.description)] unexpected inputTextField.text")
        }
    }
}
