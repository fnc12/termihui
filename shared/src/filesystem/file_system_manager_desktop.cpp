/**
 * Desktop (Windows/Linux) implementation of FileSystemManager
 * Uses sago::platform_folders library
 */

#if !defined(__APPLE__) && !defined(__ANDROID__)

#include <termihui/filesystem/file_system_manager.h>
#include <sago/platform_folders.h>

namespace termihui {

FileSystemManager::FileSystemManager(const std::string& appName)
    : appName(appName)
    , writablePath(std::filesystem::path(sago::getDataHome()) / this->appName) {
}

void FileSystemManager::initialize() {
    std::filesystem::create_directories(this->writablePath);
}

std::filesystem::path FileSystemManager::getWritablePath() const {
    return this->writablePath;
}

std::string FileSystemManager::getPlatformName() const {
#if defined(_WIN32)
    return "Windows";
#else
    return "Linux";
#endif
}

} // namespace termihui

#endif // !__APPLE__ && !__ANDROID__
