#include "termihui/clipboard/clipboard_manager_macos.h"
#import <AppKit/AppKit.h>

namespace termihui {

bool ClipboardManagerMacOS::copy(std::string_view text) {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        
        NSString* nsText = [[NSString alloc] initWithBytes:text.data()
                                                    length:text.size()
                                                  encoding:NSUTF8StringEncoding];
        if (!nsText) {
            return false;
        }
        
        return [pasteboard setString:nsText forType:NSPasteboardTypeString];
    }
}

} // namespace termihui
