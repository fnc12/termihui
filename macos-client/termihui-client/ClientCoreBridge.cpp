#include "termihui/client_core.h"

extern "C" {

bool termihui_create_app(void) {
    return termihui::createApp();
}

}
