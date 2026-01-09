#pragma once

#include <filesystem>
#include <string>

namespace termihui {

/**
 * Platform-specific file system manager.
 * 
 * Implementation is selected at compile time:
 * - Apple (macOS/iOS): file_system_manager_apple.mm
 * - Desktop (Windows/Linux): file_system_manager_desktop.cpp
 * 
 * Platform-specific locations:
 * - macOS: ~/Library/Application Support/<appName>/
 * - iOS: <App>/Documents/<appName>/
 * - Windows: %APPDATA%/<appName>/
 * - Linux: ~/.local/share/<appName>/
 */
class FileSystemManager {
public:
    explicit FileSystemManager(const std::string& appName = "termihui");
    
    /**
     * Initialize the file system manager.
     * Creates necessary directories if they don't exist.
     */
    void initialize();
    
    /**
     * Get the writable data directory for the application.
     */
    std::filesystem::path getWritablePath() const;
    
    /**
     * Get platform name for debugging/logging
     */
    std::string getPlatformName() const;

private:
    std::string appName;
    std::filesystem::path writablePath;
};

} // namespace termihui
