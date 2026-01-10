#pragma once

#include <string_view>

namespace termihui {

/**
 * Abstract clipboard manager interface.
 * Platform-specific implementations handle native clipboard operations.
 */
class ClipboardManager {
public:
    virtual ~ClipboardManager() = default;
    
    /**
     * Copy text to system clipboard
     * @param text Text to copy
     * @return true if successful
     */
    virtual bool copy(std::string_view text) = 0;
};

} // namespace termihui
