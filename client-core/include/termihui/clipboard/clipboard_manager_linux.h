#pragma once

#include "clipboard_manager.h"

namespace termihui {

class ClipboardManagerLinux : public ClipboardManager {
public:
    bool copy(std::string_view text) override;
};

} // namespace termihui
