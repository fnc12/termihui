/**
 * Apple (macOS/iOS) implementation of FileSystemManager
 * Uses Foundation framework to get proper directories
 */

#if defined(__APPLE__)

#include <TargetConditionals.h>
#include <termihui/filesystem/file_system_manager.h>
#include <Foundation/Foundation.h>

namespace termihui {

FileSystemManager::FileSystemManager(const std::string& appName)
    : appName(appName) {
    @autoreleasepool {
        NSFileManager* fileManager = [NSFileManager defaultManager];
        
#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH
        // iOS: Use Documents directory (visible in Files app, backed up)
        NSArray<NSURL*>* urls = [fileManager URLsForDirectory:NSDocumentDirectory
                                                    inDomains:NSUserDomainMask];
#else
        // macOS: Use Application Support directory
        NSArray<NSURL*>* urls = [fileManager URLsForDirectory:NSApplicationSupportDirectory
                                                    inDomains:NSUserDomainMask];
#endif
        
        if (urls.count == 0) {
            // Fallback to home directory
            NSString* home = NSHomeDirectory();
            this->writablePath = std::filesystem::path([home UTF8String]) / this->appName;
        } else {
            NSURL* baseURL = urls.firstObject;
            NSString* basePath = [baseURL path];
            this->writablePath = std::filesystem::path([basePath UTF8String]) / this->appName;
        }
    }
}

void FileSystemManager::initialize() {
    std::filesystem::create_directories(this->writablePath);
}

std::filesystem::path FileSystemManager::getWritablePath() const {
    return this->writablePath;
}

std::string FileSystemManager::getPlatformName() const {
#if TARGET_OS_IOS || TARGET_OS_TV || TARGET_OS_WATCH
    return "iOS";
#else
    return "macOS";
#endif
}

} // namespace termihui

#endif // __APPLE__
