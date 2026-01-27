#include "termihui/clipboard/clipboard_manager_linux.h"

#include <cstdio>
#include <cstdlib>
#include <array>

namespace termihui {

namespace {

// Check if a command exists in PATH
bool commandExists(const char* cmd) {
    std::array<char, 256> buffer;
    std::string checkCmd = std::string("which ") + cmd + " > /dev/null 2>&1";
    return std::system(checkCmd.c_str()) == 0;
}

// Try to copy using a specific command
bool copyWith(const char* cmd, std::string_view text) {
    FILE* pipe = popen(cmd, "w");
    if (!pipe) {
        return false;
    }
    
    size_t written = fwrite(text.data(), 1, text.size(), pipe);
    int status = pclose(pipe);
    
    return written == text.size() && status == 0;
}

} // anonymous namespace

bool ClipboardManagerLinux::copy(std::string_view text) {
    // Try Wayland first (wl-copy), then X11 (xclip, xsel)
    if (commandExists("wl-copy")) {
        return copyWith("wl-copy", text);
    }
    
    if (commandExists("xclip")) {
        return copyWith("xclip -selection clipboard", text);
    }
    
    if (commandExists("xsel")) {
        return copyWith("xsel --clipboard --input", text);
    }
    
    return false;
}

} // namespace termihui
