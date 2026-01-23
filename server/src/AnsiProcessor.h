#pragma once

#include "VirtualScreen.h"
#include <termihui/text_style.h>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

namespace termihui {

/**
 * Events generated during ANSI processing
 */
namespace AnsiEvent {
    /**
     * Interactive mode (alternate screen) changed
     */
    struct InteractiveModeChanged {
        bool entered; // true = entered, false = exited
    };
    
    /**
     * Terminal title changed (OSC 0/1/2)
     */
    struct TitleChanged {
        std::string title;
    };
    
    /**
     * Terminal bell
     */
    struct Bell {};
}

using AnsiEventVariant = std::variant<
    AnsiEvent::InteractiveModeChanged,
    AnsiEvent::TitleChanged,
    AnsiEvent::Bell
>;

/**
 * ANSI escape sequence processor
 * 
 * Parses ANSI escape sequences from PTY output and applies them to VirtualScreen.
 * Supports:
 * - SGR (Select Graphic Rendition) for colors and text styles
 * - Cursor movement commands (CUU, CUD, CUF, CUB, CUP)
 * - Screen clearing (ED) and line clearing (EL)
 * - Alternate screen buffer switching (DECSET/DECRST 1049)
 */
class AnsiProcessor {
public:
    /**
     * Construct processor attached to a VirtualScreen
     * @param screen Reference to the screen to modify
     */
    explicit AnsiProcessor(VirtualScreen& screen);
    
    /**
     * Process input data from PTY
     * @param data Raw bytes from PTY output
     * @return Vector of events generated during processing
     */
    std::vector<AnsiEventVariant> process(std::string_view data);
    
    /**
     * Reset processor state (but not screen)
     */
    void reset();
    
    /**
     * Check if currently in interactive mode (alternate screen)
     */
    bool isInteractiveMode() const { return this->interactiveMode; }

private:
    enum class State {
        Normal,     // Regular text
        Escape,     // After ESC (0x1B)
        CSI,        // After ESC[
        CSIParam,   // Reading CSI parameters
        OSC,        // After ESC]
        OSCString   // Reading OSC string
    };
    
    VirtualScreen& screen;
    State state = State::Normal;
    std::string paramBuffer;
    std::string oscBuffer;
    bool interactiveMode = false;
    
    void processCharacter(char ch, std::vector<AnsiEventVariant>& events);
    void processNormalChar(char ch, std::vector<AnsiEventVariant>& events);
    void processEscapeChar(char ch);
    void processCSIChar(char ch, std::vector<AnsiEventVariant>& events);
    void processOSCChar(char ch, std::vector<AnsiEventVariant>& events);
    
    void executeCSI(char command, std::vector<AnsiEventVariant>& events);
    void executeSGR(const std::vector<int>& params);
    void executePrivateMode(char command, bool enable, std::vector<AnsiEventVariant>& events);
    void executeOSC(std::vector<AnsiEventVariant>& events);
    
    std::vector<int> parseParams() const;
    std::optional<Color> parseExtendedColor(const std::vector<int>& params, size_t& index);
};

} // namespace termihui
