#pragma once

#include <filesystem>

class FileSystemManager {
public:
    FileSystemManager();
    
    void initialize();
    
    std::filesystem::path getWritablePath() const;

private:
    std::filesystem::path writablePath;
};

