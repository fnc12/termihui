#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <hv/json.hpp>

#include <termihui/text_style.h>

namespace termihui {

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

// JSON serialization for styled segments
void to_json(nlohmann::json& j, const Color& color);
void to_json(nlohmann::json& j, const TextStyle& textStyle);
void to_json(nlohmann::json& j, const StyledSegment& styledSegment);

} // namespace termihui
