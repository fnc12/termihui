#pragma once

#include <termihui/text_style.h>
#include <string_view>
#include <vector>

namespace termihui {

/**
 * Parser for terminal output with ANSI SGR codes.
 * Converts raw terminal output into styled segments for client rendering.
 * 
 * This is used for non-interactive mode to send pre-parsed output to clients.
 * Currently stateless, but designed as a class for future extensibility (mocking, state).
 */
class OutputParser {
public:
    OutputParser() = default;
    
    /**
     * Parse terminal output with ANSI SGR codes into styled segments.
     * @param input Raw terminal output potentially containing ANSI escape sequences
     * @return Vector of styled segments ready for client rendering
     */
    std::vector<StyledSegment> parse(std::string_view input);
    
    /**
     * Reset parser state (for future stateful parsing)
     */
    void reset();

private:
    TextStyle currentStyle;
    
    void applySGRCodes(const std::vector<int>& codes);
    std::optional<Color> parseExtendedColor(const std::vector<int>& codes, size_t& index);
    static std::vector<int> parseCSIParams(std::string_view params);
};

} // namespace termihui
