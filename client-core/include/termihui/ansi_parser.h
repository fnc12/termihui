#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <variant>

namespace termihui {

/**
 * Color representation for ANSI colors
 */
struct Color {
    enum class Type {
        Standard,   // 0-7: black, red, green, yellow, blue, magenta, cyan, white
        Bright,     // 8-15: bright versions
        Indexed,    // 0-255: 256-color palette
        RGB         // 24-bit truecolor
    };
    
    Type type;
    int index;      // For Standard, Bright, Indexed
    int r, g, b;    // For RGB
    
    // Factory methods
    static Color standard(int idx) { return {Type::Standard, idx, 0, 0, 0}; }
    static Color bright(int idx) { return {Type::Bright, idx, 0, 0, 0}; }
    static Color indexed(int idx) { return {Type::Indexed, idx, 0, 0, 0}; }
    static Color rgb(int r, int g, int b) { return {Type::RGB, 0, r, g, b}; }
    
    bool operator==(const Color& other) const = default;
};

/**
 * Text style attributes
 */
struct TextStyle {
    std::optional<Color> fg;    // null = default
    std::optional<Color> bg;    // null = default
    
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool blink = false;
    bool reverse = false;
    bool hidden = false;
    bool strikethrough = false;
    
    void reset() {
        fg = std::nullopt;
        bg = std::nullopt;
        bold = dim = italic = underline = false;
        blink = reverse = hidden = strikethrough = false;
    }
    
    bool operator==(const TextStyle& other) const = default;
};

/**
 * Styled text segment
 */
struct StyledSegment {
    std::string text;
    TextStyle style;
};

/**
 * ANSI escape sequence parser
 * Converts raw terminal output with ANSI codes into styled segments
 */
class ANSIParser {
public:
    ANSIParser() = default;
    
    /**
     * Parse text with ANSI escape codes into styled segments
     * @param input Raw text with ANSI codes
     * @return Vector of styled segments
     */
    std::vector<StyledSegment> parse(std::string_view input);
    
    /**
     * Reset parser state (current style)
     */
    void reset();

private:
    TextStyle currentStyle;
    
    // Parse CSI sequence (ESC[...m)
    void parseCSISequence(std::string_view params);
    
    // Apply SGR (Select Graphic Rendition) codes
    void applySGRCodes(const std::vector<int>& codes);
    
    // Parse color from SGR codes (handles 256-color and RGB)
    std::optional<Color> parseColor(const std::vector<int>& codes, size_t& index, bool isForeground);
};

/**
 * Standard ANSI color names
 */
inline std::string_view colorName(int index) {
    static constexpr std::string_view names[] = {
        "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"
    };
    if (index >= 0 && index < 8) return names[index];
    return "unknown";
}

} // namespace termihui

