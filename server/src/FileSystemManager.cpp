#include "FileSystemManager.h"
#include <sago/platform_folders.h>

FileSystemManager::FileSystemManager()
    : writablePath(std::filesystem::path(sago::getDataHome()) / "termihui")
{
}

void FileSystemManager::initialize() {
    std::filesystem::create_directories(this->writablePath);
}

std::filesystem::path FileSystemManager::getWritablePath() const {
    return this->writablePath;
}

