#include <catch2/catch_test_macros.hpp>
#include "TermihuiServerControllerTestable.h"
#include "hv/json.hpp"

using json = nlohmann::json;

TEST_CASE("TermihuiServerController", "[handleMessage]") {
    using Testable = TermihuiServerControllerTestable;
    
    Testable controller;
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
    }
    
    SECTION("input message calls handleInputMessage") {
        message.clientId = 123;
        message.text = json{{"type", "input"}, {"text", "hello world"}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::InputCall{123, "hello world"}};
    }
    
    SECTION("completion message calls handleCompletionMessage") {
        message.clientId = 7;
        message.text = json{{"type", "completion"}, {"text", "ls"}, {"cursor_position", 2}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::CompletionCall{7, "ls", 2}};
    }
    
    SECTION("resize message calls handleResizeMessage") {
        message.clientId = 99;
        message.text = json{{"type", "resize"}, {"cols", 120}, {"rows", 40}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::ResizeCall{99, 120, 40}};
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
    }
    
    SECTION("invalid JSON") {
        message.clientId = 1;
        message.text = "not valid json";
        controller.handleMessage(message);
    }
    
    SECTION("missing type field") {
        message.clientId = 1;
        message.text = json{{"command", "ls"}}.dump();
        controller.handleMessage(message);
    }
    
    SECTION("unknown type") {
        message.clientId = 1;
        message.text = json{{"type", "unknown_type"}}.dump();
        controller.handleMessage(message);
    }
    
    SECTION("missing required field") {
        message.clientId = 1;
        message.text = json{{"type", "execute"}}.dump();  // missing "command"
        controller.handleMessage(message);
    }
    
    REQUIRE(controller.calls == expected);
}
