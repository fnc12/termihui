#include "PangoUtils.h"
#include <fmt/format.h>
#include <glibmm.h>
#include <array>
#include <string_view>

namespace {

// Standard ANSI colors (approximate web colors)
constexpr std::array<std::string_view, 8> standardColors = {
    "#000000", // black
    "#CC0000", // red
    "#4E9A06", // green
    "#C4A000", // yellow
    "#3465A4", // blue
    "#75507B", // magenta
    "#06989A", // cyan
    "#D3D7CF"  // white
};

constexpr std::array<std::string_view, 8> brightColors = {
    "#555753", // bright black (gray)
    "#EF2929", // bright red
    "#8AE234", // bright green
    "#FCE94F", // bright yellow
    "#729FCF", // bright blue
    "#AD7FA8", // bright magenta
    "#34E2E2", // bright cyan
    "#EEEEEC"  // bright white
};

std::string color256ToPango(int index) {
    if (index < 8) {
        return std::string(standardColors[index]);
    } else if (index < 16) {
        return std::string(brightColors[index - 8]);
    } else if (index < 232) {
        // 216 color cube (6x6x6)
        int i = index - 16;
        int r = ((i / 36) % 6) * 51;
        int g = ((i / 6) % 6) * 51;
        int b = (i % 6) * 51;
        return fmt::format("#{:02X}{:02X}{:02X}", r, g, b);
    } else {
        // Grayscale (24 shades)
        int gray = (index - 232) * 10 + 8;
        return fmt::format("#{:02X}{:02X}{:02X}", gray, gray, gray);
    }
}

} // anonymous namespace

std::string colorToPango(const Color& color) {
    switch (color.type) {
        case Color::Type::Standard:
            if (color.index >= 0 && color.index < 8) {
                return std::string(standardColors[color.index]);
            }
            return "#D3D7CF"; // default white
            
        case Color::Type::Bright:
            if (color.index >= 0 && color.index < 8) {
                return std::string(brightColors[color.index]);
            }
            return "#EEEEEC"; // default bright white
            
        case Color::Type::Indexed:
            return color256ToPango(color.index);
            
        case Color::Type::RGB:
            return fmt::format("#{:02X}{:02X}{:02X}", color.r, color.g, color.b);
    }
    return "#D3D7CF";
}

std::string segmentToPangoMarkup(const StyledSegment& segment) {
    if (segment.text.empty()) {
        return "";
    }
    
    // Escape special characters for Pango markup
    std::string escapedText = Glib::Markup::escape_text(segment.text);
    
    const auto& style = segment.style;
    
    // Build span attributes
    std::string attrs;
    
    // Foreground color
    if (style.foreground.has_value()) {
        attrs += fmt::format(" foreground=\"{}\"", colorToPango(*style.foreground));
    }
    
    // Background color
    if (style.background.has_value()) {
        attrs += fmt::format(" background=\"{}\"", colorToPango(*style.background));
    }
    
    // Bold
    if (style.bold) {
        attrs += " weight=\"bold\"";
    }
    
    // Dim (use alpha)
    if (style.dim) {
        attrs += " alpha=\"50%\"";
    }
    
    // Italic
    if (style.italic) {
        attrs += " style=\"italic\"";
    }
    
    // Underline
    if (style.underline) {
        attrs += " underline=\"single\"";
    }
    
    // Strikethrough
    if (style.strikethrough) {
        attrs += " strikethrough=\"true\"";
    }
    
    // Reverse (swap fg/bg) - handle specially
    if (style.reverse) {
        std::string fg = style.background.has_value() 
            ? colorToPango(*style.background) : "#000000";
        std::string bg = style.foreground.has_value() 
            ? colorToPango(*style.foreground) : "#D3D7CF";
        // Remove existing fg/bg and add swapped
        attrs = fmt::format(" foreground=\"{}\" background=\"{}\"", fg, bg);
        if (style.bold) attrs += " weight=\"bold\"";
        if (style.italic) attrs += " style=\"italic\"";
        if (style.underline) attrs += " underline=\"single\"";
        if (style.strikethrough) attrs += " strikethrough=\"true\"";
    }
    
    if (attrs.empty()) {
        return escapedText;
    }
    
    return fmt::format("<span{}>{}</span>", attrs, escapedText);
}

std::string segmentsToPangoMarkup(const std::vector<StyledSegment>& segments) {
    std::string result;
    for (const auto& segment : segments) {
        result += segmentToPangoMarkup(segment);
    }
    return result;
}

std::string segmentsToPlainText(const std::vector<StyledSegment>& segments) {
    std::string result;
    for (const auto& segment : segments) {
        result += segment.text;
    }
    return result;
}
