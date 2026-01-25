#include "AnsiProcessor.h"
#include <charconv>
#include <algorithm>

namespace termihui {

AnsiProcessor::AnsiProcessor(VirtualScreen& screen)
    : screen(screen)
{
}

std::vector<AnsiEventVariant> AnsiProcessor::process(std::string_view data) {
    std::vector<AnsiEventVariant> events;
    
    for (char ch : data) {
        this->processCharacter(ch, events);
    }
    
    return events;
}

void AnsiProcessor::reset() {
    this->state = State::Normal;
    this->paramBuffer.clear();
    this->oscBuffer.clear();
    this->utf8Buffer.clear();
}

void AnsiProcessor::processCharacter(char ch, std::vector<AnsiEventVariant>& events) {
    switch (this->state) {
        case State::Normal:
            this->processNormalChar(ch, events);
            break;
        case State::Escape:
            this->processEscapeChar(ch);
            break;
        case State::CSI:
        case State::CSIParam:
            this->processCSIChar(ch, events);
            break;
        case State::OSC:
        case State::OSCString:
            this->processOSCChar(ch, events);
            break;
    }
}

void AnsiProcessor::processNormalChar(char ch, std::vector<AnsiEventVariant>& events) {
    unsigned char uch = static_cast<unsigned char>(ch);
    
    // Check if we're in the middle of a UTF-8 sequence
    if (!this->utf8Buffer.empty()) {
        // UTF-8 continuation byte should be 10xxxxxx (0x80-0xBF)
        if ((uch & 0xC0) == 0x80) {
            this->utf8Buffer.push_back(ch);
            
            // Check if UTF-8 sequence is complete
            size_t expectedLen = 0;
            unsigned char firstByte = static_cast<unsigned char>(this->utf8Buffer[0]);
            if ((firstByte & 0xE0) == 0xC0) expectedLen = 2;      // 110xxxxx
            else if ((firstByte & 0xF0) == 0xE0) expectedLen = 3; // 1110xxxx
            else if ((firstByte & 0xF8) == 0xF0) expectedLen = 4; // 11110xxx
            
            if (this->utf8Buffer.size() >= expectedLen) {
                // Decode UTF-8 to char32_t
                char32_t codepoint = 0;
                if (expectedLen == 2) {
                    codepoint = ((this->utf8Buffer[0] & 0x1F) << 6) |
                                (this->utf8Buffer[1] & 0x3F);
                } else if (expectedLen == 3) {
                    codepoint = ((this->utf8Buffer[0] & 0x0F) << 12) |
                                ((this->utf8Buffer[1] & 0x3F) << 6) |
                                (this->utf8Buffer[2] & 0x3F);
                } else if (expectedLen == 4) {
                    codepoint = ((this->utf8Buffer[0] & 0x07) << 18) |
                                ((this->utf8Buffer[1] & 0x3F) << 12) |
                                ((this->utf8Buffer[2] & 0x3F) << 6) |
                                (this->utf8Buffer[3] & 0x3F);
                }
                this->screen.putCharacter(codepoint);
                this->utf8Buffer.clear();
            }
            return;
        } else {
            // Invalid continuation - discard buffer and process current byte normally
            this->utf8Buffer.clear();
        }
    }
    
    // Check for UTF-8 multi-byte sequence start
    if ((uch & 0xE0) == 0xC0 || (uch & 0xF0) == 0xE0 || (uch & 0xF8) == 0xF0) {
        // Start of multi-byte UTF-8 sequence
        this->utf8Buffer.push_back(ch);
        return;
    }
    
    if (ch == '\x1B') {
        // ESC - start escape sequence
        this->state = State::Escape;
    } else if (uch == 0x9B) {
        // 8-bit CSI
        this->state = State::CSI;
        this->paramBuffer.clear();
    } else if (ch == '\r') {
        // Carriage return
        this->screen.carriageReturn();
    } else if (ch == '\n') {
        // Line feed
        this->screen.lineFeed();
    } else if (ch == '\t') {
        // Tab - move to next tab stop (every 8 columns)
        size_t currentCol = this->screen.cursorColumn();
        size_t nextTab = ((currentCol / 8) + 1) * 8;
        this->screen.moveCursor(this->screen.cursorRow(), nextTab);
    } else if (ch == '\b') {
        // Backspace
        if (this->screen.cursorColumn() > 0) {
            this->screen.moveCursorRelative(0, -1);
        }
    } else if (ch == '\x07') {
        // Bell
        events.push_back(AnsiEvent::Bell{});
    } else if (uch >= 0x20 && uch < 0x80) {
        // ASCII printable character
        this->screen.putCharacter(static_cast<char32_t>(uch));
    }
    // Other control characters and invalid bytes are ignored
}

void AnsiProcessor::processEscapeChar(char ch) {
    if (ch == '[') {
        // CSI sequence
        this->state = State::CSI;
        this->paramBuffer.clear();
    } else if (ch == ']') {
        // OSC sequence
        this->state = State::OSC;
        this->oscBuffer.clear();
    } else if (ch == '\\') {
        // ST (String Terminator) - end of sequence
        this->state = State::Normal;
    } else if (ch == 'c') {
        // RIS - Reset to Initial State
        this->screen.clearScreen(VirtualScreen::ClearScreenMode::Entire);
        this->screen.moveCursor(0, 0);
        this->screen.resetStyle();
        this->state = State::Normal;
    } else if (ch == 'D') {
        // IND - Index (line feed)
        this->screen.lineFeed();
        this->state = State::Normal;
    } else if (ch == 'E') {
        // NEL - Next Line
        this->screen.carriageReturn();
        this->screen.lineFeed();
        this->state = State::Normal;
    } else if (ch == 'M') {
        // RI - Reverse Index (reverse line feed)
        if (this->screen.cursorRow() > 0) {
            this->screen.moveCursorRelative(-1, 0);
        } else {
            this->screen.scroll(-1);
        }
        this->state = State::Normal;
    } else if (ch == '7') {
        // DECSC - Save Cursor (simplified - just ignore for now)
        this->state = State::Normal;
    } else if (ch == '8') {
        // DECRC - Restore Cursor (simplified - just ignore for now)
        this->state = State::Normal;
    } else {
        // Unknown escape sequence - ignore and return to normal
        this->state = State::Normal;
    }
}

void AnsiProcessor::processCSIChar(char ch, std::vector<AnsiEventVariant>& events) {
    unsigned char uch = static_cast<unsigned char>(ch);
    
    // Check if this is a final character (0x40-0x7E)
    if (uch >= 0x40 && uch <= 0x7E) {
        this->executeCSI(ch, events);
        this->state = State::Normal;
    } else {
        // Parameter character - accumulate
        this->paramBuffer += ch;
    }
}

void AnsiProcessor::processOSCChar(char ch, std::vector<AnsiEventVariant>& events) {
    if (ch == '\x07') {
        // BEL terminates OSC
        this->executeOSC(events);
        this->state = State::Normal;
    } else if (ch == '\x1B') {
        // Possible ST (ESC \) - will be handled in processEscapeChar
        // For now, execute OSC and switch to Escape state
        this->executeOSC(events);
        this->state = State::Escape;
    } else {
        this->oscBuffer += ch;
    }
}

void AnsiProcessor::executeOSC(std::vector<AnsiEventVariant>& events) {
    if (this->oscBuffer.empty()) {
        return;
    }
    
    // Find the separator between command and argument
    size_t semicolonPos = this->oscBuffer.find(';');
    if (semicolonPos == std::string::npos) {
        this->oscBuffer.clear();
        return;
    }
    
    std::string commandStr = this->oscBuffer.substr(0, semicolonPos);
    std::string argument = this->oscBuffer.substr(semicolonPos + 1);
    
    int command = 0;
    auto [ptr, ec] = std::from_chars(commandStr.data(), commandStr.data() + commandStr.size(), command);
    
    if (ec == std::errc()) {
        switch (command) {
            case 0: // Set icon name and window title
            case 1: // Set icon name
            case 2: // Set window title
                events.push_back(AnsiEvent::TitleChanged{std::move(argument)});
                break;
            default:
                // Other OSC commands - ignore
                break;
        }
    }
    
    this->oscBuffer.clear();
}

void AnsiProcessor::executeCSI(char command, std::vector<AnsiEventVariant>& events) {
    // Check for private mode sequences (starts with ?)
    if (!this->paramBuffer.empty() && this->paramBuffer[0] == '?') {
        std::string privateParams = this->paramBuffer.substr(1);
        this->paramBuffer = privateParams;
        
        if (command == 'h') {
            this->executePrivateMode(command, true, events);
        } else if (command == 'l') {
            this->executePrivateMode(command, false, events);
        }
        return;
    }
    
    std::vector<int> params = this->parseParams();
    
    switch (command) {
        case 'A': { // CUU - Cursor Up
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursorRelative(-n, 0);
            break;
        }
        case 'B': { // CUD - Cursor Down
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursorRelative(n, 0);
            break;
        }
        case 'C': { // CUF - Cursor Forward
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursorRelative(0, n);
            break;
        }
        case 'D': { // CUB - Cursor Back
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursorRelative(0, -n);
            break;
        }
        case 'E': { // CNL - Cursor Next Line
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursorRelative(n, 0);
            this->screen.carriageReturn();
            break;
        }
        case 'F': { // CPL - Cursor Previous Line
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursorRelative(-n, 0);
            this->screen.carriageReturn();
            break;
        }
        case 'G': { // CHA - Cursor Horizontal Absolute
            int col = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursor(this->screen.cursorRow(), static_cast<size_t>(col - 1));
            break;
        }
        case 'H':
        case 'f': { // CUP - Cursor Position
            int row = (params.size() >= 1 && params[0] > 0) ? params[0] : 1;
            int col = (params.size() >= 2 && params[1] > 0) ? params[1] : 1;
            this->screen.moveCursor(static_cast<size_t>(row - 1), static_cast<size_t>(col - 1));
            break;
        }
        case 'J': { // ED - Erase in Display
            int mode = params.empty() ? 0 : params[0];
            switch (mode) {
                case 0:
                    this->screen.clearScreen(VirtualScreen::ClearScreenMode::ToEnd);
                    break;
                case 1:
                    this->screen.clearScreen(VirtualScreen::ClearScreenMode::ToStart);
                    break;
                case 2:
                case 3: // 3 = clear scrollback too, but we treat same as 2
                    this->screen.clearScreen(VirtualScreen::ClearScreenMode::Entire);
                    break;
            }
            break;
        }
        case 'K': { // EL - Erase in Line
            int mode = params.empty() ? 0 : params[0];
            switch (mode) {
                case 0:
                    this->screen.clearLine(VirtualScreen::ClearLineMode::ToEnd);
                    break;
                case 1:
                    this->screen.clearLine(VirtualScreen::ClearLineMode::ToStart);
                    break;
                case 2:
                    this->screen.clearLine(VirtualScreen::ClearLineMode::Entire);
                    break;
            }
            break;
        }
        case 'S': { // SU - Scroll Up
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.scroll(n);
            break;
        }
        case 'T': { // SD - Scroll Down
            int n = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.scroll(-n);
            break;
        }
        case 'd': { // VPA - Vertical Position Absolute
            int row = params.empty() ? 1 : std::max(1, params[0]);
            this->screen.moveCursor(static_cast<size_t>(row - 1), this->screen.cursorColumn());
            break;
        }
        case 'm': { // SGR - Select Graphic Rendition
            if (params.empty()) {
                params.push_back(0); // Default is reset
            }
            this->executeSGR(params);
            break;
        }
        case 'r': { // DECSTBM - Set Scrolling Region (simplified - ignore)
            break;
        }
        case 's': { // Save cursor position (simplified - ignore)
            break;
        }
        case 'u': { // Restore cursor position (simplified - ignore)
            break;
        }
        default:
            // Unknown command - ignore
            break;
    }
}

void AnsiProcessor::executeSGR(const std::vector<int>& params) {
    TextStyle style = this->screen.currentStyle();
    
    size_t i = 0;
    while (i < params.size()) {
        int code = params[i];
        
        switch (code) {
            case 0:  // Reset
                style.reset();
                break;
            case 1:  // Bold
                style.bold = true;
                break;
            case 2:  // Dim
                style.dim = true;
                break;
            case 3:  // Italic
                style.italic = true;
                break;
            case 4:  // Underline
                style.underline = true;
                break;
            case 5:  // Blink
            case 6:  // Rapid blink
                style.blink = true;
                break;
            case 7:  // Reverse
                style.reverse = true;
                break;
            case 8:  // Hidden
                style.hidden = true;
                break;
            case 9:  // Strikethrough
                style.strikethrough = true;
                break;
                
            // Reset attributes
            case 22: // Not bold, not dim
                style.bold = false;
                style.dim = false;
                break;
            case 23: // Not italic
                style.italic = false;
                break;
            case 24: // Not underline
                style.underline = false;
                break;
            case 25: // Not blink
                style.blink = false;
                break;
            case 27: // Not reverse
                style.reverse = false;
                break;
            case 28: // Not hidden
                style.hidden = false;
                break;
            case 29: // Not strikethrough
                style.strikethrough = false;
                break;
                
            // Standard foreground colors (30-37)
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                style.foreground = Color::standard(code - 30);
                break;
                
            // Extended foreground color (38;5;N or 38;2;R;G;B)
            case 38:
                style.foreground = this->parseExtendedColor(params, i);
                break;
                
            // Default foreground
            case 39:
                style.foreground = std::nullopt;
                break;
                
            // Standard background colors (40-47)
            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                style.background = Color::standard(code - 40);
                break;
                
            // Extended background color (48;5;N or 48;2;R;G;B)
            case 48:
                style.background = this->parseExtendedColor(params, i);
                break;
                
            // Default background
            case 49:
                style.background = std::nullopt;
                break;
                
            // Bright foreground colors (90-97)
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                style.foreground = Color::bright(code - 90);
                break;
                
            // Bright background colors (100-107)
            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                style.background = Color::bright(code - 100);
                break;
                
            default:
                // Unknown code - ignore
                break;
        }
        
        ++i;
    }
    
    this->screen.setCurrentStyle(style);
}

void AnsiProcessor::executePrivateMode(char /* command */, bool enable, std::vector<AnsiEventVariant>& events) {
    std::vector<int> params = this->parseParams();
    
    for (int param : params) {
        switch (param) {
            case 1049: // Alternate screen buffer
            case 47:   // Alternate screen buffer (older)
            case 1047: // Alternate screen buffer variant
                if (enable != this->interactiveMode) {
                    this->interactiveMode = enable;
                    events.push_back(AnsiEvent::InteractiveModeChanged{enable});
                    if (enable) {
                        // Entering alternate screen - clear it
                        this->screen.clearScreen(VirtualScreen::ClearScreenMode::Entire);
                        this->screen.moveCursor(0, 0);
                    }
                }
                break;
            case 25: // DECTCEM - Show/hide cursor (ignore for now)
                break;
            case 7: // DECAWM - Autowrap mode (ignore for now)
                break;
            case 12: // Start/stop blinking cursor (ignore)
                break;
            default:
                // Unknown private mode - ignore
                break;
        }
    }
}

std::vector<int> AnsiProcessor::parseParams() const {
    std::vector<int> result;
    
    if (this->paramBuffer.empty()) {
        return result;
    }
    
    size_t pos = 0;
    while (pos < this->paramBuffer.size()) {
        size_t semicolonPos = this->paramBuffer.find(';', pos);
        if (semicolonPos == std::string::npos) {
            semicolonPos = this->paramBuffer.size();
        }
        
        std::string_view token{this->paramBuffer.data() + pos, semicolonPos - pos};
        
        int value = 0;
        auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (ec == std::errc()) {
            result.push_back(value);
        } else {
            result.push_back(0);
        }
        
        pos = semicolonPos + 1;
    }
    
    return result;
}

std::optional<Color> AnsiProcessor::parseExtendedColor(const std::vector<int>& params, size_t& index) {
    if (index + 1 >= params.size()) {
        return std::nullopt;
    }
    
    int colorType = params[index + 1];
    
    if (colorType == 5) {
        // 256-color mode: 38;5;N or 48;5;N
        if (index + 2 >= params.size()) {
            return std::nullopt;
        }
        int colorIndex = params[index + 2];
        index += 2;
        
        if (colorIndex >= 0 && colorIndex < 8) {
            return Color::standard(colorIndex);
        } else if (colorIndex >= 8 && colorIndex < 16) {
            return Color::bright(colorIndex - 8);
        } else {
            return Color::indexed(colorIndex);
        }
    } else if (colorType == 2) {
        // RGB mode: 38;2;R;G;B or 48;2;R;G;B
        if (index + 4 >= params.size()) {
            return std::nullopt;
        }
        int r = params[index + 2];
        int g = params[index + 3];
        int b = params[index + 4];
        index += 4;
        
        return Color::rgb(r, g, b);
    }
    
    return std::nullopt;
}

} // namespace termihui
