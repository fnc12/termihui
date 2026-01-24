#pragma once

#include <optional>
#include <string>
#include <string_view>

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
    static Color standard(int index) { return {Type::Standard, index, 0, 0, 0}; }
    static Color bright(int index) { return {Type::Bright, index, 0, 0, 0}; }
    static Color indexed(int index) { return {Type::Indexed, index, 0, 0, 0}; }
    static Color rgb(int r, int g, int b) { return {Type::RGB, 0, r, g, b}; }
    
    bool operator==(const Color& other) const = default;
};

/**
 * Text style attributes
 */
struct TextStyle {
    std::optional<Color> foreground;    // null = default
    std::optional<Color> background;    // null = default
    
    bool bold = false;
    bool dim = false;
    bool italic = false;
    bool underline = false;
    bool blink = false;
    bool reverse = false;
    bool hidden = false;
    bool strikethrough = false;
    
    void reset() {
        foreground = std::nullopt;
        background = std::nullopt;
        bold = dim = italic = underline = false;
        blink = reverse = hidden = strikethrough = false;
    }
    
    bool operator==(const TextStyle& other) const = default;
};

/**
 * Styled text segment (for protocol messages)
 */
struct StyledSegment {
    std::string text;
    TextStyle style;
    
    bool operator==(const StyledSegment& other) const = default;
};

/**
 * Terminal cell (character + style)
 */
struct Cell {
    char32_t character = U' ';
    TextStyle style;
    
    bool operator==(const Cell& other) const = default;
    
    static Cell blank() { return Cell{U' ', TextStyle{}}; }
    static Cell withCharacter(char32_t ch) { return Cell{ch, TextStyle{}}; }
    static Cell withCharacter(char32_t ch, const TextStyle& style) { return Cell{ch, style}; }
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
