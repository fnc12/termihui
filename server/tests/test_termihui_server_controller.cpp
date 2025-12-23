#include <catch2/catch_test_macros.hpp>
#include "TermihuiServerControllerTestable.h"
#include "WebSocketServerMock.h"
#include "hv/json.hpp"

using json = nlohmann::json;

TEST_CASE("TermihuiServerController", "[handleMessage]") {
    using Testable = TermihuiServerControllerTestable;
    
    auto webSocketServerMock = std::make_unique<WebSocketServerMock>();
    WebSocketServerMock* mockPtr = webSocketServerMock.get();
    
    Testable controller(std::move(webSocketServerMock));
    controller.mockHandleExecuteMessage = true;
    controller.mockHandleInputMessage = true;
    controller.mockHandleCompletionMessage = true;
    controller.mockHandleResizeMessage = true;
    
    WebSocketServer::IncomingMessage message;
    std::vector<Testable::Call> expected;
    
    SECTION("execute message calls handleExecuteMessage") {
        message.clientId = 42;
        message.text = json{{"type", "execute"}, {"command", "ls -la"}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::ExecuteCall{42, "ls -la"}};
        REQUIRE(mockPtr->calls.empty());
    }
    
    SECTION("input message calls handleInputMessage") {
        message.clientId = 123;
        message.text = json{{"type", "input"}, {"text", "hello world"}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::InputCall{123, "hello world"}};
        REQUIRE(mockPtr->calls.empty());
    }
    
    SECTION("completion message calls handleCompletionMessage") {
        message.clientId = 7;
        message.text = json{{"type", "completion"}, {"text", "ls"}, {"cursor_position", 2}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::CompletionCall{7, "ls", 2}};
        REQUIRE(mockPtr->calls.empty());
    }
    
    SECTION("resize message calls handleResizeMessage") {
        message.clientId = 99;
        message.text = json{{"type", "resize"}, {"cols", 120}, {"rows", 40}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::ResizeCall{99, 120, 40}};
        REQUIRE(mockPtr->calls.empty());
    }
    
    SECTION("multiple messages are recorded in order") {
        message.clientId = 1;
        message.text = json{{"type", "execute"}, {"command", "pwd"}}.dump();
        controller.handleMessage(message);
        
        message.clientId = 2;
        message.text = json{{"type", "input"}, {"text", "\n"}}.dump();
        controller.handleMessage(message);
        
        message.clientId = 3;
        message.text = json{{"type", "resize"}, {"cols", 80}, {"rows", 24}}.dump();
        controller.handleMessage(message);
        
        expected = {
            Testable::ExecuteCall{1, "pwd"},
            Testable::InputCall{2, "\n"},
            Testable::ResizeCall{3, 80, 24}
        };
        REQUIRE(mockPtr->calls.empty());
    }
    
    SECTION("invalid JSON sends error") {
        message.clientId = 1;
        message.text = "not valid json";
        controller.handleMessage(message);
        
        REQUIRE(mockPtr->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&mockPtr->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 1);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
    }
    
    SECTION("missing type field sends error") {
        message.clientId = 2;
        message.text = json{{"command", "ls"}}.dump();
        controller.handleMessage(message);
        
        REQUIRE(mockPtr->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&mockPtr->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 2);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
    }
    
    SECTION("unknown type sends error") {
        message.clientId = 3;
        message.text = json{{"type", "unknown_type"}}.dump();
        controller.handleMessage(message);
        
        REQUIRE(mockPtr->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&mockPtr->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 3);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
        REQUIRE(sendCall->message.find("unknown_type") != std::string::npos);
    }
    
    SECTION("missing required field sends error") {
        message.clientId = 4;
        message.text = json{{"type", "execute"}}.dump();  // missing "command"
        controller.handleMessage(message);
        
        REQUIRE(mockPtr->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&mockPtr->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 4);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
    }
    
    REQUIRE(controller.calls == expected);
}
