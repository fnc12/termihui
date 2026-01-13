#include "termihui/clipboard/clipboard_manager_ios.h"
#import <UIKit/UIKit.h>

namespace termihui {

bool ClipboardManagerIOS::copy(std::string_view text) {
    @autoreleasepool {
        NSString* nsText = [[NSString alloc] initWithBytes:text.data()
                                                    length:text.size()
                                                  encoding:NSUTF8StringEncoding];
        if (!nsText) {
            return false;
        }
        
        [UIPasteboard generalPasteboard].string = nsText;
        return true;
    }
}

} // namespace termihui
