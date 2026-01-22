#include "termihui/ansi_parser.h"
#include <charconv>
#include <fmt/core.h>

namespace termihui {

std::vector<StyledSegment> ANSIParser::parse(std::string_view input) {
    std::vector<StyledSegment> segments;
    std::string currentText;
    
    size_t i = 0;
    while (i < input.size()) {
        // Check for ESC character (0x1B)
        if (input[i] == '\x1B') {
            // Check for CSI sequence (ESC[)
            if (i + 1 < input.size() && input[i + 1] == '[') {
                // Save accumulated text as segment
                if (!currentText.empty()) {
                    segments.push_back({std::move(currentText), this->currentStyle});
                    currentText.clear();
                }
                
                // Find end of CSI sequence
                size_t start = i + 2;
                size_t end = start;
                
                // CSI sequence ends with a letter (0x40-0x7E)
                while (end < input.size() && (input[end] < 0x40 || input[end] > 0x7E)) {
                    ++end;
                }
                
                if (end < input.size()) {
                    char command = input[end];
                    std::string_view params{input.data() + start, end - start};
                    
                    // Only process SGR (Select Graphic Rendition) - 'm' command
                    if (command == 'm') {
                        parseCSISequence(params);
                    }
                    // Skip other CSI commands (cursor movement, etc.)
                    
                    i = end + 1;
                    continue;
                }
            }
            // Check for OSC sequence (ESC]) - skip it
            else if (i + 1 < input.size() && input[i + 1] == ']') {
                // Find terminator: BEL (0x07) or ST (ESC\)
                size_t end = i + 2;
                while (end < input.size()) {
                    if (input[end] == '\x07') {
                        i = end + 1;
                        break;
                    }
                    if (input[end] == '\x1B' && end + 1 < input.size() && input[end + 1] == '\\') {
                        i = end + 2;
                        break;
                    }
                    ++end;
                }
                if (end >= input.size()) {
                    i = end;
                }
                continue;
            }
            // Other ESC sequences - skip 2 characters
            else if (i + 1 < input.size()) {
                i += 2;
                continue;
            }
        }
        
        // Check for 8-bit CSI (0x9B)
        if (static_cast<unsigned char>(input[i]) == 0x9B) {
            if (!currentText.empty()) {
                segments.push_back({std::move(currentText), this->currentStyle});
                currentText.clear();
            }
            
            size_t start = i + 1;
            size_t end = start;
            
            while (end < input.size() && (input[end] < 0x40 || input[end] > 0x7E)) {
                ++end;
            }
            
            if (end < input.size()) {
                char command = input[end];
                std::string_view params{input.data() + start, end - start};
                
                if (command == 'm') {
                    parseCSISequence(params);
                }
                
                i = end + 1;
                continue;
            }
        }
        
        // Regular character
        currentText += input[i];
        ++i;
    }
    
    // Add remaining text
    if (!currentText.empty()) {
        segments.push_back({std::move(currentText), this->currentStyle});
    }
    
    return segments;
}

void ANSIParser::reset() {
    this->currentStyle.reset();
}

void ANSIParser::parseCSISequence(std::string_view params) {
    std::vector<int> codes;
    
    if (params.empty()) {
        // ESC[m is equivalent to ESC[0m (reset)
        codes.push_back(0);
    } else {
        // Parse semicolon-separated numbers using string_view
        size_t pos = 0;
        while (pos < params.size()) {
            // Find next semicolon or end
            size_t semicolonPos = params.find(';', pos);
            if (semicolonPos == std::string_view::npos) {
                semicolonPos = params.size();
            }
            
            std::string_view token = params.substr(pos, semicolonPos - pos);
            
            int value = 0;
            auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
            if (ec == std::errc()) {
                codes.push_back(value);
            } else {
                codes.push_back(0);
            }
            
            pos = semicolonPos + 1;
        }
    }
    
    applySGRCodes(codes);
}

void ANSIParser::applySGRCodes(const std::vector<int>& codes) {
    size_t i = 0;
    while (i < codes.size()) {
        int code = codes[i];
        
        switch (code) {
            case 0:  // Reset
                this->currentStyle.reset();
                break;
            case 1:  // Bold
                this->currentStyle.bold = true;
                break;
            case 2:  // Dim
                this->currentStyle.dim = true;
                break;
            case 3:  // Italic
                this->currentStyle.italic = true;
                break;
            case 4:  // Underline
                this->currentStyle.underline = true;
                break;
            case 5:  // Blink
            case 6:  // Rapid blink
                this->currentStyle.blink = true;
                break;
            case 7:  // Reverse
                this->currentStyle.reverse = true;
                break;
            case 8:  // Hidden
                this->currentStyle.hidden = true;
                break;
            case 9:  // Strikethrough
                this->currentStyle.strikethrough = true;
                break;
            
            // Reset attributes
            case 22: // Not bold, not dim
                this->currentStyle.bold = false;
                this->currentStyle.dim = false;
                break;
            case 23: // Not italic
                this->currentStyle.italic = false;
                break;
            case 24: // Not underline
                this->currentStyle.underline = false;
                break;
            case 25: // Not blink
                this->currentStyle.blink = false;
                break;
            case 27: // Not reverse
                this->currentStyle.reverse = false;
                break;
            case 28: // Not hidden
                this->currentStyle.hidden = false;
                break;
            case 29: // Not strikethrough
                this->currentStyle.strikethrough = false;
                break;
            
            // Standard foreground colors (30-37)
            case 30: case 31: case 32: case 33:
            case 34: case 35: case 36: case 37:
                this->currentStyle.foreground = Color::standard(code - 30);
                break;
            
            // Extended foreground color (38;5;N or 38;2;R;G;B)
            case 38:
                this->currentStyle.foreground = parseColor(codes, i, true);
                break;
            
            // Default foreground
            case 39:
                this->currentStyle.foreground = std::nullopt;
                break;
            
            // Standard background colors (40-47)
            case 40: case 41: case 42: case 43:
            case 44: case 45: case 46: case 47:
                this->currentStyle.background = Color::standard(code - 40);
                break;
            
            // Extended background color (48;5;N or 48;2;R;G;B)
            case 48:
                this->currentStyle.background = parseColor(codes, i, false);
                break;
            
            // Default background
            case 49:
                this->currentStyle.background = std::nullopt;
                break;
            
            // Bright foreground colors (90-97)
            case 90: case 91: case 92: case 93:
            case 94: case 95: case 96: case 97:
                this->currentStyle.foreground = Color::bright(code - 90);
                break;
            
            // Bright background colors (100-107)
            case 100: case 101: case 102: case 103:
            case 104: case 105: case 106: case 107:
                this->currentStyle.background = Color::bright(code - 100);
                break;
            
            default:
                // Unknown code - ignore
                break;
        }
        
        ++i;
    }
}

std::optional<Color> ANSIParser::parseColor(const std::vector<int>& codes, size_t& index, bool /* isForeground */) {
    // Need at least one more parameter
    if (index + 1 >= codes.size()) {
        return std::nullopt;
    }
    
    int colorType = codes[index + 1];
    
    if (colorType == 5) {
        // 256-color mode: 38;5;N or 48;5;N
        if (index + 2 >= codes.size()) {
            return std::nullopt;
        }
        int colorIndex = codes[index + 2];
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
        if (index + 4 >= codes.size()) {
            return std::nullopt;
        }
        int r = codes[index + 2];
        int g = codes[index + 3];
        int b = codes[index + 4];
        index += 4;
        
        return Color::rgb(r, g, b);
    }
    
    return std::nullopt;
}

// JSON serialization

void to_json(nlohmann::json& j, const Color& color) {
    switch (color.type) {
        case Color::Type::Standard:
            j = std::string(colorName(color.index));
            break;
        case Color::Type::Bright:
            j = std::string("bright_") + std::string(colorName(color.index));
            break;
        case Color::Type::Indexed:
            j = nlohmann::json{{"index", color.index}};
            break;
        case Color::Type::RGB:
            j = nlohmann::json{{"rgb", fmt::format("#{:02X}{:02X}{:02X}", color.r, color.g, color.b)}};
            break;
    }
}

void to_json(nlohmann::json& j, const TextStyle& textStyle) {
    j["fg"] = textStyle.foreground ? nlohmann::json(*textStyle.foreground) : nlohmann::json(nullptr);
    j["bg"] = textStyle.background ? nlohmann::json(*textStyle.background) : nlohmann::json(nullptr);
    j["bold"] = textStyle.bold;
    j["dim"] = textStyle.dim;
    j["italic"] = textStyle.italic;
    j["underline"] = textStyle.underline;
    j["reverse"] = textStyle.reverse;
    j["strikethrough"] = textStyle.strikethrough;
}

void to_json(nlohmann::json& j, const StyledSegment& styledSegment) {
    j = nlohmann::json{
        {"text", styledSegment.text},
        {"style", styledSegment.style}
    };
}

} // namespace termihui
