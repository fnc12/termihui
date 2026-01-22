#include <catch2/catch_test_macros.hpp>
#include "../src/VirtualScreen.h"

using namespace termihui;

// =============================================================================
// Construction Tests
// =============================================================================

TEST_CASE("VirtualScreen default construction creates 24x80 screen", "[VirtualScreen][construction]") {
    VirtualScreen screen;
    
    CHECK(screen.rows() == 24);
    CHECK(screen.columns() == 80);
    CHECK(screen.cursorRow() == 0);
    CHECK(screen.cursorColumn() == 0);
}

TEST_CASE("VirtualScreen construction with custom dimensions", "[VirtualScreen][construction]") {
    VirtualScreen screen(10, 40);
    
    CHECK(screen.rows() == 10);
    CHECK(screen.columns() == 40);
}

TEST_CASE("VirtualScreen initializes with blank cells", "[VirtualScreen][construction]") {
    VirtualScreen screen(5, 10);
    
    for (size_t row = 0; row < 5; ++row) {
        for (size_t col = 0; col < 10; ++col) {
            CHECK(screen.cellAt(row, col).character == U' ');
        }
    }
}

// =============================================================================
// Character Output Tests
// =============================================================================

TEST_CASE("VirtualScreen putCharacter writes at cursor and advances", "[VirtualScreen][output]") {
    VirtualScreen screen(5, 10);
    
    screen.putCharacter('H');
    screen.putCharacter('i');
    
    CHECK(screen.cellAt(0, 0).character == U'H');
    CHECK(screen.cellAt(0, 1).character == U'i');
    CHECK(screen.cursorColumn() == 2);
    CHECK(screen.cursorRow() == 0);
}

TEST_CASE("VirtualScreen putCharacter wraps at end of line", "[VirtualScreen][output]") {
    VirtualScreen screen(5, 5);
    
    for (int i = 0; i < 5; ++i) {
        screen.putCharacter('A' + i);
    }
    
    // Cursor should still be on first line, at column 5 (one past end)
    CHECK(screen.cursorRow() == 0);
    CHECK(screen.cursorColumn() == 5);
    
    // Writing another character should wrap
    screen.putCharacter('F');
    CHECK(screen.cursorRow() == 1);
    CHECK(screen.cursorColumn() == 1);
    CHECK(screen.cellAt(1, 0).character == U'F');
}

TEST_CASE("VirtualScreen putCharacter with style", "[VirtualScreen][output]") {
    VirtualScreen screen(5, 10);
    
    TextStyle style;
    style.bold = true;
    style.foreground = Color::standard(1); // Red
    
    screen.putCharacter('X', style);
    
    const Cell& cell = screen.cellAt(0, 0);
    CHECK(cell.character == U'X');
    CHECK(cell.style.bold == true);
    CHECK(cell.style.foreground.has_value());
}

TEST_CASE("VirtualScreen setCurrentStyle affects subsequent characters", "[VirtualScreen][output]") {
    VirtualScreen screen(5, 10);
    
    TextStyle style;
    style.italic = true;
    screen.setCurrentStyle(style);
    
    screen.putCharacter('A');
    screen.putCharacter('B');
    
    CHECK(screen.cellAt(0, 0).style.italic == true);
    CHECK(screen.cellAt(0, 1).style.italic == true);
}

// =============================================================================
// Cursor Movement Tests
// =============================================================================

TEST_CASE("VirtualScreen moveCursor sets absolute position", "[VirtualScreen][cursor]") {
    VirtualScreen screen(10, 20);
    
    screen.moveCursor(5, 10);
    
    CHECK(screen.cursorRow() == 5);
    CHECK(screen.cursorColumn() == 10);
}

TEST_CASE("VirtualScreen moveCursor clamps to screen bounds", "[VirtualScreen][cursor]") {
    VirtualScreen screen(10, 20);
    
    screen.moveCursor(100, 200);
    
    CHECK(screen.cursorRow() == 9);   // rows - 1
    CHECK(screen.cursorColumn() == 19); // columns - 1
}

TEST_CASE("VirtualScreen moveCursorRelative moves correctly", "[VirtualScreen][cursor]") {
    VirtualScreen screen(10, 20);
    screen.moveCursor(5, 10);
    
    screen.moveCursorRelative(-2, 3);
    CHECK(screen.cursorRow() == 3);
    CHECK(screen.cursorColumn() == 13);
    
    screen.moveCursorRelative(1, -5);
    CHECK(screen.cursorRow() == 4);
    CHECK(screen.cursorColumn() == 8);
}

TEST_CASE("VirtualScreen moveCursorRelative clamps at edges", "[VirtualScreen][cursor]") {
    VirtualScreen screen(10, 20);
    screen.moveCursor(2, 3);
    
    // Try to move past top-left
    screen.moveCursorRelative(-100, -100);
    CHECK(screen.cursorRow() == 0);
    CHECK(screen.cursorColumn() == 0);
    
    // Try to move past bottom-right
    screen.moveCursorRelative(100, 100);
    CHECK(screen.cursorRow() == 9);
    CHECK(screen.cursorColumn() == 19);
}

TEST_CASE("VirtualScreen carriageReturn moves to column 0", "[VirtualScreen][cursor]") {
    VirtualScreen screen(5, 10);
    screen.moveCursor(2, 7);
    
    screen.carriageReturn();
    
    CHECK(screen.cursorRow() == 2);
    CHECK(screen.cursorColumn() == 0);
}

TEST_CASE("VirtualScreen lineFeed moves to next row", "[VirtualScreen][cursor]") {
    VirtualScreen screen(5, 10);
    screen.moveCursor(2, 5);
    
    screen.lineFeed();
    
    CHECK(screen.cursorRow() == 3);
    CHECK(screen.cursorColumn() == 5); // Column unchanged
}

TEST_CASE("VirtualScreen lineFeed at bottom scrolls", "[VirtualScreen][cursor]") {
    VirtualScreen screen(3, 10);
    
    // Write identifiable content on each line
    screen.moveCursor(0, 0);
    screen.putCharacter('A');
    screen.moveCursor(1, 0);
    screen.putCharacter('B');
    screen.moveCursor(2, 0);
    screen.putCharacter('C');
    
    // Move to bottom and do lineFeed
    screen.moveCursor(2, 0);
    screen.lineFeed();
    
    // Screen should have scrolled
    CHECK(screen.cellAt(0, 0).character == U'B');
    CHECK(screen.cellAt(1, 0).character == U'C');
    CHECK(screen.cellAt(2, 0).character == U' '); // New blank line
}

// =============================================================================
// Clear Line Tests
// =============================================================================

TEST_CASE("VirtualScreen clearLine ToEnd clears from cursor", "[VirtualScreen][clear]") {
    VirtualScreen screen(3, 10);
    
    // Fill line
    for (int i = 0; i < 10; ++i) {
        screen.putCharacter('A' + i);
    }
    screen.moveCursor(0, 5);
    
    screen.clearLine(VirtualScreen::ClearLineMode::ToEnd);
    
    // Characters before cursor preserved
    CHECK(screen.cellAt(0, 0).character == U'A');
    CHECK(screen.cellAt(0, 4).character == U'E');
    // Characters from cursor cleared
    CHECK(screen.cellAt(0, 5).character == U' ');
    CHECK(screen.cellAt(0, 9).character == U' ');
}

TEST_CASE("VirtualScreen clearLine ToStart clears to cursor", "[VirtualScreen][clear]") {
    VirtualScreen screen(3, 10);
    
    for (int i = 0; i < 10; ++i) {
        screen.putCharacter('A' + i);
    }
    screen.moveCursor(0, 5);
    
    screen.clearLine(VirtualScreen::ClearLineMode::ToStart);
    
    // Characters up to and including cursor cleared
    CHECK(screen.cellAt(0, 0).character == U' ');
    CHECK(screen.cellAt(0, 5).character == U' ');
    // Characters after cursor preserved
    CHECK(screen.cellAt(0, 6).character == U'G');
    CHECK(screen.cellAt(0, 9).character == U'J');
}

TEST_CASE("VirtualScreen clearLine Entire clears whole line", "[VirtualScreen][clear]") {
    VirtualScreen screen(3, 10);
    
    for (int i = 0; i < 10; ++i) {
        screen.putCharacter('A' + i);
    }
    screen.moveCursor(0, 5);
    
    screen.clearLine(VirtualScreen::ClearLineMode::Entire);
    
    for (size_t col = 0; col < 10; ++col) {
        CHECK(screen.cellAt(0, col).character == U' ');
    }
}

// =============================================================================
// Clear Screen Tests
// =============================================================================

TEST_CASE("VirtualScreen clearScreen Entire clears all", "[VirtualScreen][clear]") {
    VirtualScreen screen(3, 5);
    
    // Fill screen
    for (size_t row = 0; row < 3; ++row) {
        screen.moveCursor(row, 0);
        for (size_t col = 0; col < 5; ++col) {
            screen.putCharacter('X');
        }
    }
    
    screen.clearScreen(VirtualScreen::ClearScreenMode::Entire);
    
    for (size_t row = 0; row < 3; ++row) {
        for (size_t col = 0; col < 5; ++col) {
            CHECK(screen.cellAt(row, col).character == U' ');
        }
    }
}

// =============================================================================
// Scroll Tests
// =============================================================================

TEST_CASE("VirtualScreen scroll up moves content", "[VirtualScreen][scroll]") {
    VirtualScreen screen(3, 5);
    
    screen.moveCursor(0, 0); screen.putCharacter('A');
    screen.moveCursor(1, 0); screen.putCharacter('B');
    screen.moveCursor(2, 0); screen.putCharacter('C');
    
    screen.scroll(1);
    
    CHECK(screen.cellAt(0, 0).character == U'B');
    CHECK(screen.cellAt(1, 0).character == U'C');
    CHECK(screen.cellAt(2, 0).character == U' ');
}

TEST_CASE("VirtualScreen scroll down moves content", "[VirtualScreen][scroll]") {
    VirtualScreen screen(3, 5);
    
    screen.moveCursor(0, 0); screen.putCharacter('A');
    screen.moveCursor(1, 0); screen.putCharacter('B');
    screen.moveCursor(2, 0); screen.putCharacter('C');
    
    screen.scroll(-1);
    
    CHECK(screen.cellAt(0, 0).character == U' ');
    CHECK(screen.cellAt(1, 0).character == U'A');
    CHECK(screen.cellAt(2, 0).character == U'B');
}

// =============================================================================
// Resize Tests
// =============================================================================

TEST_CASE("VirtualScreen resize preserves content", "[VirtualScreen][resize]") {
    VirtualScreen screen(3, 5);
    
    screen.putCharacter('H');
    screen.putCharacter('i');
    
    screen.resize(5, 10);
    
    CHECK(screen.rows() == 5);
    CHECK(screen.columns() == 10);
    CHECK(screen.cellAt(0, 0).character == U'H');
    CHECK(screen.cellAt(0, 1).character == U'i');
}

TEST_CASE("VirtualScreen resize clamps cursor", "[VirtualScreen][resize]") {
    VirtualScreen screen(10, 20);
    screen.moveCursor(8, 15);
    
    screen.resize(5, 10);
    
    CHECK(screen.cursorRow() == 4);
    CHECK(screen.cursorColumn() == 9);
}

// =============================================================================
// Content Access Tests
// =============================================================================

TEST_CASE("VirtualScreen getRowText returns text without trailing spaces", "[VirtualScreen][content]") {
    VirtualScreen screen(3, 10);
    
    screen.putCharacter('H');
    screen.putCharacter('e');
    screen.putCharacter('l');
    screen.putCharacter('l');
    screen.putCharacter('o');
    
    std::string row = screen.getRowText(0);
    CHECK(row == "Hello");
}

TEST_CASE("VirtualScreen getContent returns all rows", "[VirtualScreen][content]") {
    VirtualScreen screen(3, 10);
    
    screen.moveCursor(0, 0);
    screen.putCharacter('A');
    screen.moveCursor(1, 0);
    screen.putCharacter('B');
    screen.moveCursor(2, 0);
    screen.putCharacter('C');
    
    std::string content = screen.getContent();
    CHECK(content == "A\nB\nC");
}

// =============================================================================
// Dirty Tracking Tests
// =============================================================================

TEST_CASE("VirtualScreen tracks dirty rows", "[VirtualScreen][dirty]") {
    VirtualScreen screen(5, 10);
    
    screen.clearDirtyRows();
    CHECK(screen.dirtyRows().empty());
    
    screen.moveCursor(2, 0);
    screen.putCharacter('X');
    
    CHECK(screen.dirtyRows().count(2) == 1);
    CHECK(screen.dirtyRows().size() == 1);
    
    screen.clearDirtyRows();
    CHECK(screen.dirtyRows().empty());
}

TEST_CASE("VirtualScreen markAllDirty marks all rows", "[VirtualScreen][dirty]") {
    VirtualScreen screen(5, 10);
    
    screen.clearDirtyRows();
    screen.markAllDirty();
    
    CHECK(screen.dirtyRows().size() == 5);
    for (size_t row = 0; row < 5; ++row) {
        CHECK(screen.dirtyRows().count(row) == 1);
    }
}
