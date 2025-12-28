#include <catch2/catch_test_macros.hpp>
#include "ClientCoreControllerTestable.h"
#include <hv/json.hpp>

using json = nlohmann::json;
using namespace termihui;

TEST_CASE("ClientCoreController", "[sendMessage]") {
    using Testable = ClientCoreControllerTestable;
    
    Testable controller;
    std::vector<Testable::Call> expected;
    
    SECTION("initialized") {
        controller.initialize();
        
        SECTION("connectButtonClicked calls handleConnectButtonClicked") {
            controller.sendMessage(json{{"type", "connectButtonClicked"}, {"address", "ws://localhost:8080"}}.dump());
            expected = {Testable::ConnectButtonClickedCall{"ws://localhost:8080"}};
        }
        
        SECTION("requestReconnect calls handleRequestReconnect") {
            controller.sendMessage(json{{"type", "requestReconnect"}, {"address", "ws://192.168.1.1:9000"}}.dump());
            expected = {Testable::RequestReconnectCall{"ws://192.168.1.1:9000"}};
        }
        
        SECTION("disconnectButtonClicked calls handleDisconnectButtonClicked") {
            controller.sendMessage(json{{"type", "disconnectButtonClicked"}}.dump());
            expected = {Testable::DisconnectButtonClickedCall{}};
        }
        
        SECTION("executeCommand calls handleExecuteCommand") {
            controller.sendMessage(json{{"type", "executeCommand"}, {"command", "ls -la"}}.dump());
            expected = {Testable::ExecuteCommandCall{"ls -la"}};
        }
        
        SECTION("sendInput calls handleSendInput") {
            controller.sendMessage(json{{"type", "sendInput"}, {"text", "hello world"}}.dump());
            expected = {Testable::SendInputCall{"hello world"}};
        }
        
        SECTION("resize calls handleResize") {
            controller.sendMessage(json{{"type", "resize"}, {"cols", 120}, {"rows", 40}}.dump());
            expected = {Testable::ResizeCall{120, 40}};
        }
        
        SECTION("requestCompletion calls handleRequestCompletion") {
            controller.sendMessage(json{{"type", "requestCompletion"}, {"text", "ls"}, {"cursorPosition", 2}}.dump());
            expected = {Testable::RequestCompletionCall{"ls", 2}};
        }
        
        SECTION("multiple messages are recorded in order") {
            controller.sendMessage(json{{"type", "executeCommand"}, {"command", "pwd"}}.dump());
            controller.sendMessage(json{{"type", "sendInput"}, {"text", "\n"}}.dump());
            controller.sendMessage(json{{"type", "resize"}, {"cols", 80}, {"rows", 24}}.dump());
            
            expected = {
                Testable::ExecuteCommandCall{"pwd"},
                Testable::SendInputCall{"\n"},
                Testable::ResizeCall{80, 24}
            };
        }
        
        SECTION("invalid JSON - calls is empty") {
            controller.sendMessage("not valid json");
        }
        
        SECTION("missing type field - calls is empty") {
            controller.sendMessage(json{{"command", "ls"}}.dump());
        }
        
        SECTION("unknown type - calls is empty") {
            controller.sendMessage(json{{"type", "unknown_type"}}.dump());
        }
        
        SECTION("missing required field - calls is empty") {
            controller.sendMessage(json{{"type", "executeCommand"}}.dump());
        }
    }
    
    SECTION("not initialized") {
        controller.sendMessage(json{{"type", "executeCommand"}, {"command", "ls"}}.dump());
    }
    
    REQUIRE(controller.calls == expected);
}
