#include "VirtualScreen.h"
#include <algorithm>

namespace termihui {

VirtualScreen::VirtualScreen(size_t rows, size_t columns)
    : buffer(rows, columns, Cell::blank())
    , rowCount(rows)
    , columnCount(columns)
{
}

VirtualScreen::VirtualScreen()
    : VirtualScreen(24, 80)
{
}

// =============================================================================
// Character Output
// =============================================================================

void VirtualScreen::putCharacter(char32_t character) {
    this->putCharacter(character, this->currentTextStyle);
}

void VirtualScreen::putCharacter(char32_t character, const TextStyle& style) {
    if (this->cursorColumnPosition >= this->columnCount) {
        // Wrap to next line
        this->cursorColumnPosition = 0;
        this->lineFeed();
    }
    
    this->buffer(this->cursorRowPosition, this->cursorColumnPosition) = Cell{character, style};
    this->markDirty(this->cursorRowPosition);
    ++this->cursorColumnPosition;
    this->cursorDirtyFlag = true;
}

void VirtualScreen::setCurrentStyle(const TextStyle& style) {
    this->currentTextStyle = style;
}

void VirtualScreen::resetStyle() {
    this->currentTextStyle.reset();
}

// =============================================================================
// Cursor Movement
// =============================================================================

void VirtualScreen::moveCursor(size_t row, size_t column) {
    this->cursorRowPosition = std::min(row, this->rowCount > 0 ? this->rowCount - 1 : 0);
    this->cursorColumnPosition = std::min(column, this->columnCount > 0 ? this->columnCount - 1 : 0);
    this->cursorDirtyFlag = true;
}

void VirtualScreen::moveCursorRelative(int deltaRow, int deltaColumn) {
    // Handle row movement
    if (deltaRow < 0) {
        size_t absMove = static_cast<size_t>(-deltaRow);
        this->cursorRowPosition = (this->cursorRowPosition >= absMove) ? this->cursorRowPosition - absMove : 0;
    } else {
        this->cursorRowPosition = std::min(this->cursorRowPosition + static_cast<size_t>(deltaRow), this->rowCount - 1);
    }
    
    // Handle column movement
    if (deltaColumn < 0) {
        size_t absMove = static_cast<size_t>(-deltaColumn);
        this->cursorColumnPosition = (this->cursorColumnPosition >= absMove) ? this->cursorColumnPosition - absMove : 0;
    } else {
        this->cursorColumnPosition = std::min(this->cursorColumnPosition + static_cast<size_t>(deltaColumn), this->columnCount - 1);
    }
    
    this->cursorDirtyFlag = true;
}

void VirtualScreen::carriageReturn() {
    this->cursorColumnPosition = 0;
    this->cursorDirtyFlag = true;
}

void VirtualScreen::lineFeed() {
    if (this->cursorRowPosition + 1 < this->rowCount) {
        ++this->cursorRowPosition;
    } else {
        // At bottom - scroll up
        this->scroll(1);
    }
    this->cursorDirtyFlag = true;
}

// =============================================================================
// Screen Manipulation
// =============================================================================

void VirtualScreen::clearLine(ClearLineMode mode) {
    Cell blank = this->blankCell();
    
    switch (mode) {
        case ClearLineMode::ToEnd:
            // Clear from cursor to end of line
            for (size_t col = this->cursorColumnPosition; col < this->columnCount; ++col) {
                this->buffer(this->cursorRowPosition, col) = blank;
            }
            break;
            
        case ClearLineMode::ToStart:
            // Clear from start of line to cursor (inclusive)
            for (size_t col = 0; col <= this->cursorColumnPosition; ++col) {
                this->buffer(this->cursorRowPosition, col) = blank;
            }
            break;
            
        case ClearLineMode::Entire:
            // Clear entire line
            this->buffer.fillRow(this->cursorRowPosition, blank);
            break;
    }
    
    this->markDirty(this->cursorRowPosition);
}

void VirtualScreen::clearScreen(ClearScreenMode mode) {
    Cell blank = this->blankCell();
    
    switch (mode) {
        case ClearScreenMode::ToEnd:
            // Clear from cursor to end of screen
            // First, clear rest of current line
            for (size_t col = this->cursorColumnPosition; col < this->columnCount; ++col) {
                this->buffer(this->cursorRowPosition, col) = blank;
            }
            this->markDirty(this->cursorRowPosition);
            
            // Then clear all lines below
            for (size_t row = this->cursorRowPosition + 1; row < this->rowCount; ++row) {
                this->buffer.fillRow(row, blank);
                this->markDirty(row);
            }
            break;
            
        case ClearScreenMode::ToStart:
            // Clear from start of screen to cursor
            // First, clear all lines above
            for (size_t row = 0; row < this->cursorRowPosition; ++row) {
                this->buffer.fillRow(row, blank);
                this->markDirty(row);
            }
            
            // Then clear current line up to and including cursor
            for (size_t col = 0; col <= this->cursorColumnPosition; ++col) {
                this->buffer(this->cursorRowPosition, col) = blank;
            }
            this->markDirty(this->cursorRowPosition);
            break;
            
        case ClearScreenMode::Entire:
            // Clear entire screen
            this->buffer.fill(blank);
            this->markAllDirty();
            break;
    }
}

void VirtualScreen::scroll(int lines) {
    if (lines == 0) return;
    
    Cell blank = this->blankCell();
    
    if (lines > 0) {
        // Scroll up (content moves up, new blank lines at bottom)
        size_t scrollAmount = std::min(static_cast<size_t>(lines), this->rowCount);
        
        // Capture rows that will be pushed off the top before overwriting
        for (size_t row = 0; row < scrollAmount; ++row) {
            this->scrolledOffRows.push_back(this->getRowSegments(row));
        }
        
        // Move lines up
        for (size_t row = 0; row < this->rowCount - scrollAmount; ++row) {
            for (size_t col = 0; col < this->columnCount; ++col) {
                this->buffer(row, col) = this->buffer(row + scrollAmount, col);
            }
            this->markDirty(row);
        }
        
        // Clear bottom lines
        for (size_t row = this->rowCount - scrollAmount; row < this->rowCount; ++row) {
            this->buffer.fillRow(row, blank);
            this->markDirty(row);
        }
    } else {
        // Scroll down (content moves down, new blank lines at top)
        size_t scrollAmount = std::min(static_cast<size_t>(-lines), this->rowCount);
        
        // Move lines down (start from bottom)
        for (size_t row = this->rowCount - 1; row >= scrollAmount; --row) {
            for (size_t col = 0; col < this->columnCount; ++col) {
                this->buffer(row, col) = this->buffer(row - scrollAmount, col);
            }
            this->markDirty(row);
            if (row == 0) break; // Prevent underflow
        }
        
        // Clear top lines
        for (size_t row = 0; row < scrollAmount; ++row) {
            this->buffer.fillRow(row, blank);
            this->markDirty(row);
        }
    }
}

void VirtualScreen::resize(size_t rows, size_t columns) {
    if (rows == this->rowCount && columns == this->columnCount) {
        return;
    }
    
    this->buffer.resize(rows, columns, Cell::blank());
    this->rowCount = rows;
    this->columnCount = columns;
    
    // Ensure cursor is in bounds
    this->ensureCursorInBounds();
    this->markAllDirty();
}

// =============================================================================
// Content Access
// =============================================================================

const Cell& VirtualScreen::cellAt(size_t row, size_t column) const {
    return this->buffer.at(row, column);
}

// Helper: encode char32_t to UTF-8
static void appendUtf8(std::string& result, char32_t ch) {
    if (ch == 0) ch = U' ';
    
    if (ch < 128) {
        result += static_cast<char>(ch);
    } else if (ch < 0x800) {
        result += static_cast<char>(0xC0 | (ch >> 6));
        result += static_cast<char>(0x80 | (ch & 0x3F));
    } else if (ch < 0x10000) {
        result += static_cast<char>(0xE0 | (ch >> 12));
        result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (ch & 0x3F));
    } else {
        result += static_cast<char>(0xF0 | (ch >> 18));
        result += static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (ch & 0x3F));
    }
}

std::string VirtualScreen::getRowText(size_t row) const {
    std::string result;
    result.reserve(this->columnCount);
    
    // Find last non-space character
    size_t lastNonSpace = 0;
    for (size_t col = 0; col < this->columnCount; ++col) {
        char32_t ch = this->buffer(row, col).character;
        if (ch != U' ' && ch != 0) {
            lastNonSpace = col + 1;
        }
    }
    
    // Build string up to last non-space
    for (size_t col = 0; col < lastNonSpace; ++col) {
        appendUtf8(result, this->buffer(row, col).character);
    }
    
    return result;
}

std::vector<StyledSegment> VirtualScreen::getRowSegments(size_t row, bool trimTrailingSpaces) const {
    std::vector<StyledSegment> segments;
    
    if (row >= this->rowCount) {
        return segments;
    }
    
    // Find last non-space character if trimming
    size_t endCol = this->columnCount;
    if (trimTrailingSpaces) {
        endCol = 0;
        for (size_t col = 0; col < this->columnCount; ++col) {
            char32_t ch = this->buffer(row, col).character;
            if (ch != U' ' && ch != 0) {
                endCol = col + 1;
            }
        }
    }
    
    if (endCol == 0) {
        return segments;
    }
    
    // Group adjacent cells with same style
    StyledSegment current;
    current.style = this->buffer(row, 0).style;
    
    for (size_t col = 0; col < endCol; ++col) {
        const Cell& cell = this->buffer(row, col);
        
        if (cell.style == current.style) {
            // Same style - append to current segment
            appendUtf8(current.text, cell.character);
        } else {
            // Style changed - save current and start new
            if (!current.text.empty()) {
                segments.push_back(std::move(current));
            }
            current = StyledSegment{};
            current.style = cell.style;
            appendUtf8(current.text, cell.character);
        }
    }
    
    // Don't forget last segment
    if (!current.text.empty()) {
        segments.push_back(std::move(current));
    }
    
    return segments;
}

std::string VirtualScreen::getContent(bool includeTrailingSpaces) const {
    std::string result;
    
    for (size_t row = 0; row < this->rowCount; ++row) {
        if (row > 0) {
            result += '\n';
        }
        
        if (includeTrailingSpaces) {
            // Include all characters
            for (size_t col = 0; col < this->columnCount; ++col) {
                char32_t ch = this->buffer(row, col).character;
                if (ch == 0) ch = U' ';
                if (ch < 128) {
                    result += static_cast<char>(ch);
                }
            }
        } else {
            result += this->getRowText(row);
        }
    }
    
    return result;
}

// =============================================================================
// Change Tracking
// =============================================================================

void VirtualScreen::clearDirtyRows() {
    this->dirtyRowSet.clear();
    this->cursorDirtyFlag = false;
}

void VirtualScreen::markAllDirty() {
    for (size_t row = 0; row < this->rowCount; ++row) {
        this->dirtyRowSet.insert(row);
    }
}

// =============================================================================
// Scroll-off Capture
// =============================================================================

std::vector<std::vector<StyledSegment>> VirtualScreen::takeScrolledOffRows() {
    auto result = std::move(this->scrolledOffRows);
    this->scrolledOffRows.clear();
    return result;
}

// =============================================================================
// Private Methods
// =============================================================================

void VirtualScreen::markDirty(size_t row) {
    this->dirtyRowSet.insert(row);
}

void VirtualScreen::ensureCursorInBounds() {
    if (this->rowCount > 0) {
        this->cursorRowPosition = std::min(this->cursorRowPosition, this->rowCount - 1);
    } else {
        this->cursorRowPosition = 0;
    }
    
    if (this->columnCount > 0) {
        this->cursorColumnPosition = std::min(this->cursorColumnPosition, this->columnCount - 1);
    } else {
        this->cursorColumnPosition = 0;
    }
}

Cell VirtualScreen::blankCell() const {
    return Cell{U' ', this->currentTextStyle};
}

} // namespace termihui
