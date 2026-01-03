#include <catch2/catch_test_macros.hpp>
#include "ClientCoreControllerTestable.h"
#include "MockWebSocketClientController.h"
#include <hv/json.hpp>

using json = nlohmann::json;
using namespace termihui;

TEST_CASE("ClientCoreController") {
    using Testable = ClientCoreControllerTestable;
    
    auto mockWebSocketController = std::make_unique<MockWebSocketClientController>();
    auto* mockWebSocketControllerPtr = mockWebSocketController.get();
    Testable controller(std::move(mockWebSocketController));
    
    std::vector<Testable::Call> expectedCalls;
    int expectedUpdateCallCount = 0;
    
    SECTION("sendMessage") {
        SECTION("initialized") {
            controller.initialize();
            
            SECTION("connectButtonClicked calls handleConnectButtonClicked") {
                controller.sendMessage(json{{"type", "connectButtonClicked"}, {"address", "ws://localhost:8080"}}.dump());
                expectedCalls = {Testable::ConnectButtonClickedCall{"ws://localhost:8080"}};
            }
            
            SECTION("requestReconnect calls handleRequestReconnect") {
                controller.sendMessage(json{{"type", "requestReconnect"}, {"address", "ws://192.168.1.1:9000"}}.dump());
                expectedCalls = {Testable::RequestReconnectCall{"ws://192.168.1.1:9000"}};
            }
            
            SECTION("disconnectButtonClicked calls handleDisconnectButtonClicked") {
                controller.sendMessage(json{{"type", "disconnectButtonClicked"}}.dump());
                expectedCalls = {Testable::DisconnectButtonClickedCall{}};
            }
            
            SECTION("executeCommand calls handleExecuteCommand") {
                controller.sendMessage(json{{"type", "executeCommand"}, {"command", "ls -la"}}.dump());
                expectedCalls = {Testable::ExecuteCommandCall{"ls -la"}};
            }
            
            SECTION("sendInput calls handleSendInput") {
                controller.sendMessage(json{{"type", "sendInput"}, {"text", "hello world"}}.dump());
                expectedCalls = {Testable::SendInputCall{"hello world"}};
            }
            
            SECTION("resize calls handleResize") {
                controller.sendMessage(json{{"type", "resize"}, {"cols", 120}, {"rows", 40}}.dump());
                expectedCalls = {Testable::ResizeCall{120, 40}};
            }
            
            SECTION("requestCompletion calls handleRequestCompletion") {
                controller.sendMessage(json{{"type", "requestCompletion"}, {"text", "ls"}, {"cursorPosition", 2}}.dump());
                expectedCalls = {Testable::RequestCompletionCall{"ls", 2}};
            }
            
            SECTION("multiple messages are recorded in order") {
                controller.sendMessage(json{{"type", "executeCommand"}, {"command", "pwd"}}.dump());
                controller.sendMessage(json{{"type", "sendInput"}, {"text", "\n"}}.dump());
                controller.sendMessage(json{{"type", "resize"}, {"cols", 80}, {"rows", 24}}.dump());
                
                expectedCalls = {
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
    }
    
    SECTION("update") {
        SECTION("empty events - no handleWebSocketEvent calls") {
            mockWebSocketControllerPtr->eventsToReturn = {};
            
            controller.update();
            expectedUpdateCallCount = 1;
        }
        
        SECTION("single OpenEvent") {
            mockWebSocketControllerPtr->eventsToReturn = {
                WebSocketClientController::OpenEvent{}
            };
            
            controller.update();
            expectedUpdateCallCount = 1;
            expectedCalls = {Testable::OpenEventCall{}};
        }
        
        SECTION("single MessageEvent") {
            mockWebSocketControllerPtr->eventsToReturn = {
                WebSocketClientController::MessageEvent{"hello from server"}
            };
            
            controller.update();
            expectedUpdateCallCount = 1;
            expectedCalls = {Testable::MessageEventCall{"hello from server"}};
        }
        
        SECTION("single CloseEvent") {
            mockWebSocketControllerPtr->eventsToReturn = {
                WebSocketClientController::CloseEvent{}
            };
            
            controller.update();
            expectedUpdateCallCount = 1;
            expectedCalls = {Testable::CloseEventCall{}};
        }
        
        SECTION("single ErrorEvent") {
            mockWebSocketControllerPtr->eventsToReturn = {
                WebSocketClientController::ErrorEvent{"connection refused"}
            };
            
            controller.update();
            expectedUpdateCallCount = 1;
            expectedCalls = {Testable::ErrorEventCall{"connection refused"}};
        }
        
        SECTION("two events - Open then Message") {
            mockWebSocketControllerPtr->eventsToReturn = {
                WebSocketClientController::OpenEvent{},
                WebSocketClientController::MessageEvent{"welcome"}
            };
            
            controller.update();
            expectedUpdateCallCount = 1;
            expectedCalls = {
                Testable::OpenEventCall{},
                Testable::MessageEventCall{"welcome"}
            };
        }
        
        SECTION("three events - Open, Message, Close") {
            mockWebSocketControllerPtr->eventsToReturn = {
                WebSocketClientController::OpenEvent{},
                WebSocketClientController::MessageEvent{R"({"type":"connected"})"},
                WebSocketClientController::CloseEvent{}
            };
            
            controller.update();
            expectedUpdateCallCount = 1;
            expectedCalls = {
                Testable::OpenEventCall{},
                Testable::MessageEventCall{R"({"type":"connected"})"},
                Testable::CloseEventCall{}
            };
        }
        
        SECTION("multiple update calls") {
            mockWebSocketControllerPtr->eventsToReturn = {WebSocketClientController::OpenEvent{}};
            controller.update();
            
            mockWebSocketControllerPtr->eventsToReturn = {WebSocketClientController::MessageEvent{"msg1"}};
            controller.update();
            
            mockWebSocketControllerPtr->eventsToReturn = {};
            controller.update();
            
            expectedUpdateCallCount = 3;
            expectedCalls = {
                Testable::OpenEventCall{},
                Testable::MessageEventCall{"msg1"}
            };
        }
        
        SECTION("mixed event sequence") {
            mockWebSocketControllerPtr->eventsToReturn = {
                WebSocketClientController::ErrorEvent{"timeout"},
                WebSocketClientController::OpenEvent{},
                WebSocketClientController::MessageEvent{"data1"},
                WebSocketClientController::MessageEvent{"data2"},
                WebSocketClientController::CloseEvent{}
            };
            
            controller.update();
            expectedUpdateCallCount = 1;
            expectedCalls = {
                Testable::ErrorEventCall{"timeout"},
                Testable::OpenEventCall{},
                Testable::MessageEventCall{"data1"},
                Testable::MessageEventCall{"data2"},
                Testable::CloseEventCall{}
            };
        }
    }
    
    REQUIRE(mockWebSocketControllerPtr->updateCallCount == expectedUpdateCallCount);
    REQUIRE(controller.calls == expectedCalls);
}
