#include <catch2/catch_test_macros.hpp>
#include "../src/AnsiProcessor.h"

using namespace termihui;

// =============================================================================
// Basic Text Processing
// =============================================================================

TEST_CASE("AnsiProcessor writes plain text to screen", "[AnsiProcessor][text]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("Hello");
    
    CHECK(screen.getRowText(0) == "Hello");
    CHECK(screen.cursorColumn() == 5);
}

TEST_CASE("AnsiProcessor handles carriage return", "[AnsiProcessor][text]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("Hello\rWorld");
    
    CHECK(screen.getRowText(0) == "World");
}

TEST_CASE("AnsiProcessor handles line feed", "[AnsiProcessor][text]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("Line1\nLine2");
    
    CHECK(screen.getRowText(0) == "Line1");
    CHECK(screen.getRowText(1) == "     Line2"); // Column preserved
}

TEST_CASE("AnsiProcessor handles CRLF", "[AnsiProcessor][text]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("Line1\r\nLine2");
    
    CHECK(screen.getRowText(0) == "Line1");
    CHECK(screen.getRowText(1) == "Line2");
}

TEST_CASE("AnsiProcessor handles tab", "[AnsiProcessor][text]") {
    VirtualScreen screen(5, 40);
    AnsiProcessor processor(screen);
    
    processor.process("A\tB");
    
    // Tab moves to column 8
    CHECK(screen.cellAt(0, 0).character == U'A');
    CHECK(screen.cellAt(0, 8).character == U'B');
}

TEST_CASE("AnsiProcessor handles backspace", "[AnsiProcessor][text]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("ABC\b\bX");
    
    CHECK(screen.getRowText(0) == "AXC");
}

// =============================================================================
// Cursor Movement
// =============================================================================

TEST_CASE("AnsiProcessor CUU moves cursor up", "[AnsiProcessor][cursor]") {
    VirtualScreen screen(10, 20);
    AnsiProcessor processor(screen);
    
    screen.moveCursor(5, 5);
    processor.process("\x1B[2A"); // Move up 2
    
    CHECK(screen.cursorRow() == 3);
    CHECK(screen.cursorColumn() == 5);
}

TEST_CASE("AnsiProcessor CUD moves cursor down", "[AnsiProcessor][cursor]") {
    VirtualScreen screen(10, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[3B"); // Move down 3
    
    CHECK(screen.cursorRow() == 3);
}

TEST_CASE("AnsiProcessor CUF moves cursor forward", "[AnsiProcessor][cursor]") {
    VirtualScreen screen(10, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[5C"); // Move forward 5
    
    CHECK(screen.cursorColumn() == 5);
}

TEST_CASE("AnsiProcessor CUB moves cursor back", "[AnsiProcessor][cursor]") {
    VirtualScreen screen(10, 20);
    AnsiProcessor processor(screen);
    
    screen.moveCursor(0, 10);
    processor.process("\x1B[3D"); // Move back 3
    
    CHECK(screen.cursorColumn() == 7);
}

TEST_CASE("AnsiProcessor CUP moves cursor to position", "[AnsiProcessor][cursor]") {
    VirtualScreen screen(10, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[5;10H"); // Row 5, Column 10 (1-based)
    
    CHECK(screen.cursorRow() == 4);    // 0-based
    CHECK(screen.cursorColumn() == 9); // 0-based
}

TEST_CASE("AnsiProcessor CUP with default parameters", "[AnsiProcessor][cursor]") {
    VirtualScreen screen(10, 20);
    AnsiProcessor processor(screen);
    
    screen.moveCursor(5, 5);
    processor.process("\x1B[H"); // Default is 1;1
    
    CHECK(screen.cursorRow() == 0);
    CHECK(screen.cursorColumn() == 0);
}

TEST_CASE("AnsiProcessor CHA moves to column", "[AnsiProcessor][cursor]") {
    VirtualScreen screen(10, 20);
    AnsiProcessor processor(screen);
    
    screen.moveCursor(5, 0);
    processor.process("\x1B[10G"); // Column 10 (1-based)
    
    CHECK(screen.cursorRow() == 5);
    CHECK(screen.cursorColumn() == 9); // 0-based
}

// =============================================================================
// Screen Clearing
// =============================================================================

TEST_CASE("AnsiProcessor ED clears screen to end", "[AnsiProcessor][clear]") {
    VirtualScreen screen(3, 10);
    AnsiProcessor processor(screen);
    
    processor.process("AAAAAAAAAA");
    screen.moveCursor(1, 0);
    processor.process("BBBBBBBBBB");
    screen.moveCursor(2, 0);
    processor.process("CCCCCCCCCC");
    
    screen.moveCursor(1, 5);
    processor.process("\x1B[J"); // Clear to end (default 0)
    
    CHECK(screen.getRowText(0) == "AAAAAAAAAA");
    CHECK(screen.getRowText(1) == "BBBBB");
    CHECK(screen.getRowText(2) == "");
}

TEST_CASE("AnsiProcessor ED clears entire screen", "[AnsiProcessor][clear]") {
    VirtualScreen screen(3, 10);
    AnsiProcessor processor(screen);
    
    processor.process("AAAAAAAAAA");
    screen.moveCursor(1, 0);
    processor.process("BBBBBBBBBB");
    
    processor.process("\x1B[2J"); // Clear entire screen
    
    CHECK(screen.getRowText(0) == "");
    CHECK(screen.getRowText(1) == "");
}

TEST_CASE("AnsiProcessor EL clears line to end", "[AnsiProcessor][clear]") {
    VirtualScreen screen(3, 10);
    AnsiProcessor processor(screen);
    
    processor.process("AAAAAAAAAA");
    screen.moveCursor(0, 5);
    processor.process("\x1B[K"); // Clear to end (default 0)
    
    CHECK(screen.getRowText(0) == "AAAAA");
}

TEST_CASE("AnsiProcessor EL clears entire line", "[AnsiProcessor][clear]") {
    VirtualScreen screen(3, 10);
    AnsiProcessor processor(screen);
    
    processor.process("AAAAAAAAAA");
    screen.moveCursor(0, 5);
    processor.process("\x1B[2K"); // Clear entire line
    
    CHECK(screen.getRowText(0) == "");
}

// =============================================================================
// SGR (Colors and Styles)
// =============================================================================

TEST_CASE("AnsiProcessor SGR reset", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[1;31mRed\x1B[0mNormal");
    
    CHECK(screen.cellAt(0, 0).style.bold == true);
    CHECK(screen.cellAt(0, 0).style.foreground.has_value());
    CHECK(screen.cellAt(0, 3).style.bold == false);
    CHECK(!screen.cellAt(0, 3).style.foreground.has_value());
}

TEST_CASE("AnsiProcessor SGR bold", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[1mBold");
    
    CHECK(screen.cellAt(0, 0).style.bold == true);
}

TEST_CASE("AnsiProcessor SGR standard foreground colors", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[31mR"); // Red
    
    CHECK(screen.cellAt(0, 0).style.foreground.has_value());
    CHECK(screen.cellAt(0, 0).style.foreground->type == Color::Type::Standard);
    CHECK(screen.cellAt(0, 0).style.foreground->index == 1);
}

TEST_CASE("AnsiProcessor SGR bright foreground colors", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[92mG"); // Bright green
    
    CHECK(screen.cellAt(0, 0).style.foreground.has_value());
    CHECK(screen.cellAt(0, 0).style.foreground->type == Color::Type::Bright);
    CHECK(screen.cellAt(0, 0).style.foreground->index == 2);
}

TEST_CASE("AnsiProcessor SGR 256-color foreground", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[38;5;208mO"); // Orange (index 208)
    
    CHECK(screen.cellAt(0, 0).style.foreground.has_value());
    CHECK(screen.cellAt(0, 0).style.foreground->type == Color::Type::Indexed);
    CHECK(screen.cellAt(0, 0).style.foreground->index == 208);
}

TEST_CASE("AnsiProcessor SGR RGB foreground", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[38;2;255;128;64mC"); // RGB color
    
    CHECK(screen.cellAt(0, 0).style.foreground.has_value());
    CHECK(screen.cellAt(0, 0).style.foreground->type == Color::Type::RGB);
    CHECK(screen.cellAt(0, 0).style.foreground->r == 255);
    CHECK(screen.cellAt(0, 0).style.foreground->g == 128);
    CHECK(screen.cellAt(0, 0).style.foreground->b == 64);
}

TEST_CASE("AnsiProcessor SGR background colors", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[44mB"); // Blue background
    
    CHECK(screen.cellAt(0, 0).style.background.has_value());
    CHECK(screen.cellAt(0, 0).style.background->type == Color::Type::Standard);
    CHECK(screen.cellAt(0, 0).style.background->index == 4);
}

TEST_CASE("AnsiProcessor SGR multiple attributes", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[1;3;4;31;44mX"); // Bold, italic, underline, red on blue
    
    const TextStyle& style = screen.cellAt(0, 0).style;
    CHECK(style.bold == true);
    CHECK(style.italic == true);
    CHECK(style.underline == true);
    CHECK(style.foreground.has_value());
    CHECK(style.background.has_value());
}

// =============================================================================
// Interactive Mode (Alternate Screen)
// =============================================================================

TEST_CASE("AnsiProcessor detects alternate screen enter", "[AnsiProcessor][interactive]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    auto events = processor.process("\x1B[?1049h"); // Enter alternate screen
    
    CHECK(processor.isInteractiveMode() == true);
    REQUIRE(events.size() == 1);
    
    auto* event = std::get_if<AnsiEvent::InteractiveModeChanged>(&events[0]);
    REQUIRE(event != nullptr);
    CHECK(event->entered == true);
}

TEST_CASE("AnsiProcessor detects alternate screen exit", "[AnsiProcessor][interactive]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[?1049h"); // Enter
    auto events = processor.process("\x1B[?1049l"); // Exit
    
    CHECK(processor.isInteractiveMode() == false);
    REQUIRE(events.size() == 1);
    
    auto* event = std::get_if<AnsiEvent::InteractiveModeChanged>(&events[0]);
    REQUIRE(event != nullptr);
    CHECK(event->entered == false);
}

TEST_CASE("AnsiProcessor alternate screen is idempotent", "[AnsiProcessor][interactive]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    auto events1 = processor.process("\x1B[?1049h"); // Enter
    auto events2 = processor.process("\x1B[?1049h"); // Enter again
    auto events3 = processor.process("\x1B[?1049h"); // Enter again
    
    CHECK(events1.size() == 1); // First time - event emitted
    CHECK(events2.size() == 0); // Second time - no event (already in mode)
    CHECK(events3.size() == 0); // Third time - no event
}

// =============================================================================
// OSC Sequences
// =============================================================================

TEST_CASE("AnsiProcessor handles OSC title change", "[AnsiProcessor][osc]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    auto events = processor.process("\x1B]0;Window Title\x07Text");
    
    CHECK(screen.getRowText(0) == "Text");
    REQUIRE(events.size() == 1);
    
    auto* event = std::get_if<AnsiEvent::TitleChanged>(&events[0]);
    REQUIRE(event != nullptr);
    CHECK(event->title == "Window Title");
}

TEST_CASE("AnsiProcessor handles OSC with ST terminator", "[AnsiProcessor][osc]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    auto events = processor.process("\x1B]2;My Title\x1B\\Text");
    
    CHECK(screen.getRowText(0) == "Text");
    REQUIRE(events.size() == 1);
    
    auto* event = std::get_if<AnsiEvent::TitleChanged>(&events[0]);
    REQUIRE(event != nullptr);
    CHECK(event->title == "My Title");
}

// =============================================================================
// Bell Event
// =============================================================================

TEST_CASE("AnsiProcessor emits Bell event", "[AnsiProcessor][bell]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    auto events = processor.process("Text\x07More");
    
    CHECK(screen.getRowText(0) == "TextMore");
    REQUIRE(events.size() == 1);
    CHECK(std::holds_alternative<AnsiEvent::Bell>(events[0]));
}

// =============================================================================
// 8-bit CSI
// =============================================================================

TEST_CASE("AnsiProcessor handles 8-bit CSI", "[AnsiProcessor][csi]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x9B" "31mRed"); // 0x9B = 8-bit CSI
    
    CHECK(screen.cellAt(0, 0).style.foreground.has_value());
    CHECK(screen.cellAt(0, 0).style.foreground->index == 1);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("AnsiProcessor handles empty SGR", "[AnsiProcessor][sgr]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("\x1B[1mBold\x1B[mNormal"); // ESC[m = reset
    
    CHECK(screen.cellAt(0, 0).style.bold == true);
    CHECK(screen.cellAt(0, 4).style.bold == false);
}

TEST_CASE("AnsiProcessor handles incomplete escape sequences", "[AnsiProcessor][edge]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    // Process data that ends mid-sequence
    processor.process("Text\x1B");
    processor.process("[31mRed");
    
    CHECK(screen.getRowText(0) == "TextRed");
    CHECK(screen.cellAt(0, 4).style.foreground.has_value());
}

TEST_CASE("AnsiProcessor reset clears state but not screen", "[AnsiProcessor][reset]") {
    VirtualScreen screen(5, 20);
    AnsiProcessor processor(screen);
    
    processor.process("Hello\x1B[");
    processor.reset();
    processor.process("World");
    
    CHECK(screen.getRowText(0) == "HelloWorld");
}
