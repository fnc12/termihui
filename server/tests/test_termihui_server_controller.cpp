#include <catch2/catch_test_macros.hpp>
#include "TermihuiServerControllerTestable.h"
#include "TerminalSessionControllerMock.h"
#include "WebSocketServerMock.h"
#include "AIAgentControllerMock.h"
#include "ServerStorageMock.h"
#include <termihui/protocol/protocol.h>

using json = nlohmann::json;

// Helper to create expected output message JSON with default style segments
static std::string makeExpectedOutputMessage(const std::string& text) {
    json defaultStyle = {
        {"fg", nullptr},
        {"bg", nullptr},
        {"bold", false},
        {"dim", false},
        {"italic", false},
        {"underline", false},
        {"reverse", false},
        {"strikethrough", false}
    };
    json segment = {{"text", text}, {"style", defaultStyle}};
    json message = {{"type", "output"}, {"segments", json::array({segment})}};
    return message.dump();
}

TEST_CASE("TermihuiServerController", "[handleMessage]") {
    using Testable = TermihuiServerControllerTestable;
    
    auto webSocketServerMock = std::make_unique<WebSocketServerMock>();
    auto aiAgentControllerMock = std::make_unique<AIAgentControllerMock>();
    auto serverStorageMock = std::make_unique<ServerStorageMock>();
    WebSocketServerMock* webSocketServerMockPointer = webSocketServerMock.get();
    
    Testable controller(std::move(webSocketServerMock), std::move(aiAgentControllerMock), std::move(serverStorageMock));
    controller.mockExecute = true;
    controller.mockInput = true;
    controller.mockCompletion = true;
    controller.mockResize = true;
    
    WebSocketServer::IncomingMessage message;
    std::vector<Testable::Call> expected;
    
    SECTION("execute message calls handleExecuteMessage") {
        message.clientId = 42;
        message.text = json{{"type", "execute"}, {"session_id", 5}, {"command", "ls -la"}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::ExecuteCall{.clientId = 42, .sessionId = 5, .command = "ls -la"}};
        REQUIRE(webSocketServerMockPointer->calls.empty());
    }
    
    SECTION("input message calls handleInputMessage") {
        message.clientId = 123;
        message.text = json{{"type", "input"}, {"session_id", 1}, {"text", "hello world"}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::InputCall{.clientId = 123, .sessionId = 1, .text = "hello world"}};
        REQUIRE(webSocketServerMockPointer->calls.empty());
    }
    
    SECTION("completion message calls handleCompletionMessage") {
        message.clientId = 7;
        message.text = json{{"type", "completion"}, {"session_id", 2}, {"text", "ls"}, {"cursor_position", 2}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::CompletionCall{.clientId = 7, .sessionId = 2, .text = "ls", .cursorPosition = 2}};
        REQUIRE(webSocketServerMockPointer->calls.empty());
    }
    
    SECTION("resize message calls handleResizeMessage") {
        message.clientId = 99;
        message.text = json{{"type", "resize"}, {"session_id", 3}, {"cols", 120}, {"rows", 40}}.dump();
        controller.handleMessage(message);
        
        expected = {Testable::ResizeCall{.clientId = 99, .sessionId = 3, .cols = 120, .rows = 40}};
        REQUIRE(webSocketServerMockPointer->calls.empty());
    }
    
    SECTION("multiple messages are recorded in order") {
        message.clientId = 1;
        message.text = json{{"type", "execute"}, {"session_id", 10}, {"command", "pwd"}}.dump();
        controller.handleMessage(message);
        
        message.clientId = 2;
        message.text = json{{"type", "input"}, {"session_id", 10}, {"text", "\n"}}.dump();
        controller.handleMessage(message);
        
        message.clientId = 3;
        message.text = json{{"type", "resize"}, {"session_id", 10}, {"cols", 80}, {"rows", 24}}.dump();
        controller.handleMessage(message);
        
        expected = {
            Testable::ExecuteCall{.clientId = 1, .sessionId = 10, .command = "pwd"},
            Testable::InputCall{.clientId = 2, .sessionId = 10, .text = "\n"},
            Testable::ResizeCall{.clientId = 3, .sessionId = 10, .cols = 80, .rows = 24}
        };
        REQUIRE(webSocketServerMockPointer->calls.empty());
    }
    
    SECTION("invalid JSON sends error") {
        message.clientId = 1;
        message.text = "not valid json";
        controller.handleMessage(message);
        
        REQUIRE(webSocketServerMockPointer->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&webSocketServerMockPointer->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 1);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
    }
    
    SECTION("missing type field sends error") {
        message.clientId = 2;
        message.text = json{{"command", "ls"}}.dump();
        controller.handleMessage(message);
        
        REQUIRE(webSocketServerMockPointer->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&webSocketServerMockPointer->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 2);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
    }
    
    SECTION("unknown type sends error") {
        message.clientId = 3;
        message.text = json{{"type", "unknown_type"}}.dump();
        controller.handleMessage(message);
        
        REQUIRE(webSocketServerMockPointer->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&webSocketServerMockPointer->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 3);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
        REQUIRE(sendCall->message.find("unknown_type") != std::string::npos);
    }
    
    SECTION("missing required field sends error") {
        message.clientId = 4;
        message.text = json{{"type", "execute"}}.dump();
        controller.handleMessage(message);
        
        REQUIRE(webSocketServerMockPointer->calls.size() == 1);
        auto* sendCall = std::get_if<WebSocketServerMock::SendMessageCall>(&webSocketServerMockPointer->calls[0]);
        REQUIRE(sendCall != nullptr);
        REQUIRE(sendCall->clientId == 4);
        REQUIRE(sendCall->message.find("error") != std::string::npos);
    }
    
    REQUIRE(controller.calls == expected);
}

TEST_CASE("TermihuiServerController", "[processTerminalOutput]") {
    using SessionMock = TerminalSessionControllerMock;
    using WsMock = WebSocketServerMock;
    using AiMock = AIAgentControllerMock;
    using StorageMock = ServerStorageMock;
    
    auto webSocketServerMock = std::make_unique<WsMock>();
    auto aiAgentControllerMock = std::make_unique<AiMock>();
    auto serverStorageMock = std::make_unique<StorageMock>();
    WsMock* wsMockPtr = webSocketServerMock.get();
    
    TermihuiServerControllerTestable controller(std::move(webSocketServerMock), std::move(aiAgentControllerMock), std::move(serverStorageMock));
    
    SessionMock sessionMock;
    std::vector<SessionMock::Call> expectedCalls;
    std::vector<WsMock::Call> expectedWsCalls;
    
    SECTION("no data available - does nothing") {
        sessionMock.hasDataReturnValue = false;
        controller.processTerminalOutput(sessionMock);
    }
    
    SECTION("empty output - does nothing") {
        sessionMock.hasDataReturnValue = true;
        sessionMock.readOutputReturnValues.push("");
        controller.processTerminalOutput(sessionMock);
    }
    
    SECTION("plain text output - broadcasts and appends to history") {
        sessionMock.readOutputReturnValues.push("hello world\r\n");
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::AppendOutputToCurrentCommandCall{"hello world\r\n"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{makeExpectedOutputMessage("hello world\r\n")}
        };
    }
    
    SECTION("OSC 133;A command_start - updates cwd and starts history") {
        sessionMock.readOutputReturnValues.push("\x1b]133;A;cwd=/tmp\x07");
        sessionMock.hasActiveCommandReturnValue = true;
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/tmp"},
            SessionMock::StartCommandInHistoryCall{"/tmp"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "command_start"}, {"cwd", "/tmp"}}.dump()}
        };
    }
    
    SECTION("OSC 133;B command_end - finishes history") {
        sessionMock.readOutputReturnValues.push("\x1b]133;B;exit=0;cwd=/home\x07");
        sessionMock.hasActiveCommandReturnValue = true;
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/home"},
            SessionMock::FinishCurrentCommandCall{0, "/home"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "command_end"}, {"exit_code", 0}, {"cwd", "/home"}}.dump()}
        };
    }
    
    SECTION("OSC 133;B with non-zero exit code") {
        sessionMock.readOutputReturnValues.push("\x1b]133;B;exit=127;cwd=/tmp\x07");
        sessionMock.hasActiveCommandReturnValue = true;
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/tmp"},
            SessionMock::FinishCurrentCommandCall{127, "/tmp"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "command_end"}, {"exit_code", 127}, {"cwd", "/tmp"}}.dump()}
        };
    }
    
    SECTION("pwd command - full cycle") {
        sessionMock.readOutputReturnValues.push(
            "\x1b]133;A;cwd=/Users/test\x07"
            "/Users/test\r\n"
            "\x1b]133;B;exit=0;cwd=/Users/test\x07"
        );
        sessionMock.hasActiveCommandReturnValue = true;
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/Users/test"},
            SessionMock::StartCommandInHistoryCall{"/Users/test"},
            SessionMock::AppendOutputToCurrentCommandCall{"/Users/test\r\n"},
            SessionMock::SetLastKnownCwdCall{"/Users/test"},
            SessionMock::FinishCurrentCommandCall{0, "/Users/test"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "command_start"}, {"cwd", "/Users/test"}}.dump()},
            WsMock::BroadcastMessageCall{makeExpectedOutputMessage("/Users/test\r\n")},
            WsMock::BroadcastMessageCall{json{{"type", "command_end"}, {"exit_code", 0}, {"cwd", "/Users/test"}}.dump()}
        };
    }
    
    SECTION("cd command - cwd changes") {
        sessionMock.readOutputReturnValues.push(
            "\x1b]133;A;cwd=/old/path\x07"
            "\x1b]133;B;exit=0;cwd=/new/path\x07"
        );
        sessionMock.hasActiveCommandReturnValue = true;
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/old/path"},
            SessionMock::StartCommandInHistoryCall{"/old/path"},
            SessionMock::SetLastKnownCwdCall{"/new/path"},
            SessionMock::FinishCurrentCommandCall{0, "/new/path"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "command_start"}, {"cwd", "/old/path"}}.dump()},
            WsMock::BroadcastMessageCall{json{{"type", "command_end"}, {"exit_code", 0}, {"cwd", "/new/path"}}.dump()}
        };
    }
    
    SECTION("OSC 133;C prompt_start - broadcasts event") {
        sessionMock.readOutputReturnValues.push("\x1b]133;C\x07");
        controller.processTerminalOutput(sessionMock);
        
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "prompt_start"}}.dump()}
        };
    }
    
    SECTION("OSC 133;D prompt_end - broadcasts event") {
        sessionMock.readOutputReturnValues.push("\x1b]133;D\x07");
        controller.processTerminalOutput(sessionMock);
        
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "prompt_end"}}.dump()}
        };
    }
    
    SECTION("OSC 7 cwd update - file:// format") {
        sessionMock.readOutputReturnValues.push("\x1b]7;file://localhost/Users/test\x07");
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/Users/test"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "cwd_update"}, {"cwd", "/Users/test"}}.dump()}
        };
    }
    
    SECTION("OSC 2 window title with cwd") {
        sessionMock.readOutputReturnValues.push("\x1b]2;user@host:/tmp\x07");
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/tmp"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "cwd_update"}, {"cwd", "/tmp"}}.dump()}
        };
    }
    
    SECTION("unknown OSC type - ignored") {
        sessionMock.readOutputReturnValues.push("\x1b]999;some data\x07");
        controller.processTerminalOutput(sessionMock);
    }
    
    SECTION("incomplete OSC - treated as text") {
        sessionMock.readOutputReturnValues.push("text\x1b]133;A");
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::AppendOutputToCurrentCommandCall{"text"},
            SessionMock::AppendOutputToCurrentCommandCall{"\x1b]133;A"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{makeExpectedOutputMessage("text")},
            WsMock::BroadcastMessageCall{json{{"type", "output"}, {"segments", json::array()}}.dump()}
        };
    }
    
    SECTION("multiple chunks - simulates large ls output") {
        sessionMock.readOutputReturnValues.push("\x1b]133;A;cwd=/tmp\x07");
        sessionMock.readOutputReturnValues.push("file1.txt\r\nfile2.txt\r\n");
        sessionMock.readOutputReturnValues.push("\x1b]133;B;exit=0;cwd=/tmp\x07");
        sessionMock.hasActiveCommandReturnValue = true;
        
        controller.processTerminalOutput(sessionMock);
        controller.processTerminalOutput(sessionMock);
        controller.processTerminalOutput(sessionMock);
        
        expectedCalls = {
            SessionMock::SetLastKnownCwdCall{"/tmp"},
            SessionMock::StartCommandInHistoryCall{"/tmp"},
            SessionMock::AppendOutputToCurrentCommandCall{"file1.txt\r\nfile2.txt\r\n"},
            SessionMock::SetLastKnownCwdCall{"/tmp"},
            SessionMock::FinishCurrentCommandCall{0, "/tmp"}
        };
        expectedWsCalls = {
            WsMock::BroadcastMessageCall{json{{"type", "command_start"}, {"cwd", "/tmp"}}.dump()},
            WsMock::BroadcastMessageCall{makeExpectedOutputMessage("file1.txt\r\nfile2.txt\r\n")},
            WsMock::BroadcastMessageCall{json{{"type", "command_end"}, {"exit_code", 0}, {"cwd", "/tmp"}}.dump()}
        };
    }
    
    REQUIRE(sessionMock.calls == expectedCalls);
    REQUIRE(wsMockPtr->calls == expectedWsCalls);
}

TEST_CASE("TermihuiServerController::update", "[update]") {
    using Testable = TermihuiServerControllerTestable;
    using WsMock = WebSocketServerMock;
    using AiMock = AIAgentControllerMock;
    using StorageMock = ServerStorageMock;
    
    struct TestCase {
        std::vector<AIEvent> aiUpdateReturnValue;
        std::vector<WsMock::Call> expectedWsCalls;
        std::vector<AiMock::Call> expectedAiCalls;
        std::vector<StorageMock::Call> expectedStorageCalls;
    };
    
    const std::vector<TestCase> testCases = {
        // update calls webSocketServer->update and aiAgentController->update
        {
            .aiUpdateReturnValue = {},
            .expectedWsCalls = {WsMock::UpdateCall{}},
            .expectedAiCalls = {AiMock::UpdateCall{}},
            .expectedStorageCalls = {}
        },
        // update processes AI events and broadcasts them
        {
            .aiUpdateReturnValue = {
                {AIEvent::Type::Chunk, 123, "Hello"},
                {AIEvent::Type::Done, 123, ""}
            },
            .expectedWsCalls = {
                WsMock::UpdateCall{},
                WsMock::BroadcastMessageCall{json{{"type", "ai_chunk"}, {"session_id", 123}, {"content", "Hello"}}.dump()},
                WsMock::BroadcastMessageCall{json{{"type", "ai_done"}, {"session_id", 123}}.dump()}
            },
            .expectedAiCalls = {AiMock::UpdateCall{}},
            .expectedStorageCalls = {}  // Empty content is not saved
        },
        // update processes AI Done with content - saves to storage
        {
            .aiUpdateReturnValue = {
                {AIEvent::Type::Done, 123, "Full response"}
            },
            .expectedWsCalls = {
                WsMock::UpdateCall{},
                WsMock::BroadcastMessageCall{json{{"type", "ai_done"}, {"session_id", 123}}.dump()}
            },
            .expectedAiCalls = {AiMock::UpdateCall{}},
            .expectedStorageCalls = {StorageMock::SaveChatMessageCall{123, "assistant", "Full response"}}
        },
        // update processes AI error events - saves error to storage
        {
            .aiUpdateReturnValue = {
                {AIEvent::Type::Error, 456, "Connection failed"}
            },
            .expectedWsCalls = {
                WsMock::UpdateCall{},
                WsMock::BroadcastMessageCall{json{{"type", "ai_error"}, {"session_id", 456}, {"error", "Connection failed"}}.dump()}
            },
            .expectedAiCalls = {AiMock::UpdateCall{}},
            .expectedStorageCalls = {StorageMock::SaveChatMessageCall{456, "error", "Connection failed"}}
        }
    };
    
    for (const auto& testCase : testCases) {
        auto webSocketServerMock = std::make_unique<WsMock>();
        auto aiAgentControllerMock = std::make_unique<AiMock>();
        auto serverStorageMock = std::make_unique<StorageMock>();
        
        WsMock* wsMockPtr = webSocketServerMock.get();
        AiMock* aiMockPtr = aiAgentControllerMock.get();
        StorageMock* storageMockPtr = serverStorageMock.get();
        
        aiMockPtr->updateReturnValue = testCase.aiUpdateReturnValue;
        
        Testable controller(std::move(webSocketServerMock), std::move(aiAgentControllerMock), std::move(serverStorageMock));
        controller.update();
        
        REQUIRE(wsMockPtr->calls == testCase.expectedWsCalls);
        REQUIRE(aiMockPtr->calls == testCase.expectedAiCalls);
        REQUIRE(storageMockPtr->calls == testCase.expectedStorageCalls);
    }
}
