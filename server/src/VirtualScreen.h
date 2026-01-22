#pragma once

#include <termihui/grid2d.h>
#include <termihui/text_style.h>
#include <string>
#include <set>

namespace termihui {

/**
 * Virtual terminal screen buffer
 * 
 * Maintains a 2D grid of cells representing the terminal display.
 * Supports cursor movement, text insertion, and screen manipulation operations.
 */
class VirtualScreen {
public:
    /**
     * Clear line mode for clearLine()
     */
    enum class ClearLineMode {
        ToEnd = 0,      // Clear from cursor to end of line (ESC[0K)
        ToStart = 1,    // Clear from start of line to cursor (ESC[1K)
        Entire = 2      // Clear entire line (ESC[2K)
    };
    
    /**
     * Clear screen mode for clearScreen()
     */
    enum class ClearScreenMode {
        ToEnd = 0,      // Clear from cursor to end of screen (ESC[0J)
        ToStart = 1,    // Clear from start of screen to cursor (ESC[1J)
        Entire = 2      // Clear entire screen (ESC[2J)
    };
    
    /**
     * Construct screen with given dimensions
     * @param rows Number of rows
     * @param columns Number of columns
     */
    VirtualScreen(size_t rows, size_t columns);
    
    /**
     * Default constructor (24x80 screen)
     */
    VirtualScreen();
    
    // =========================================================================
    // Character Output
    // =========================================================================
    
    /**
     * Write character at cursor position and advance cursor
     * @param character Character to write
     */
    void putCharacter(char32_t character);
    
    /**
     * Write character with specific style at cursor position
     * @param character Character to write
     * @param style Text style to apply
     */
    void putCharacter(char32_t character, const TextStyle& style);
    
    /**
     * Set current text style for subsequent characters
     * @param style Text style to use
     */
    void setCurrentStyle(const TextStyle& style);
    
    /**
     * Get current text style
     */
    const TextStyle& currentStyle() const { return this->currentTextStyle; }
    
    /**
     * Reset current style to default
     */
    void resetStyle();
    
    // =========================================================================
    // Cursor Movement
    // =========================================================================
    
    /**
     * Move cursor to absolute position
     * @param row Row (0-indexed)
     * @param column Column (0-indexed)
     */
    void moveCursor(size_t row, size_t column);
    
    /**
     * Move cursor relative to current position
     * @param deltaRow Rows to move (negative = up)
     * @param deltaColumn Columns to move (negative = left)
     */
    void moveCursorRelative(int deltaRow, int deltaColumn);
    
    /**
     * Move cursor to beginning of current line
     */
    void carriageReturn();
    
    /**
     * Move cursor to next line, scrolling if at bottom
     */
    void lineFeed();
    
    /**
     * Get current cursor row
     */
    size_t cursorRow() const { return this->cursorRowPosition; }
    
    /**
     * Get current cursor column
     */
    size_t cursorColumn() const { return this->cursorColumnPosition; }
    
    // =========================================================================
    // Screen Manipulation
    // =========================================================================
    
    /**
     * Clear part or all of current line
     * @param mode What part to clear
     */
    void clearLine(ClearLineMode mode);
    
    /**
     * Clear part or all of screen
     * @param mode What part to clear
     */
    void clearScreen(ClearScreenMode mode);
    
    /**
     * Scroll screen content
     * @param lines Number of lines to scroll (positive = up, negative = down)
     */
    void scroll(int lines);
    
    /**
     * Resize screen
     * @param rows New number of rows
     * @param columns New number of columns
     */
    void resize(size_t rows, size_t columns);
    
    // =========================================================================
    // Content Access
    // =========================================================================
    
    /**
     * Get cell at position
     * @param row Row (0-indexed)
     * @param column Column (0-indexed)
     */
    const Cell& cellAt(size_t row, size_t column) const;
    
    /**
     * Get row content as string (without trailing spaces)
     * @param row Row index
     */
    std::string getRowText(size_t row) const;
    
    /**
     * Get entire screen content as string
     * @param includeTrailingSpaces Whether to include trailing spaces on each line
     */
    std::string getContent(bool includeTrailingSpaces = false) const;
    
    /**
     * Get screen dimensions
     */
    size_t rows() const { return this->rowCount; }
    size_t columns() const { return this->columnCount; }
    
    // =========================================================================
    // Change Tracking
    // =========================================================================
    
    /**
     * Get set of rows that have been modified since last call to clearDirtyRows()
     */
    const std::set<size_t>& dirtyRows() const { return this->dirtyRowSet; }
    
    /**
     * Clear the dirty rows set
     */
    void clearDirtyRows();
    
    /**
     * Mark all rows as dirty
     */
    void markAllDirty();

private:
    Grid2D<Cell> buffer;
    size_t rowCount;
    size_t columnCount;
    size_t cursorRowPosition = 0;
    size_t cursorColumnPosition = 0;
    TextStyle currentTextStyle;
    std::set<size_t> dirtyRowSet;
    
    void markDirty(size_t row);
    void ensureCursorInBounds();
    Cell blankCell() const;
};

} // namespace termihui
