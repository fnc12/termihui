#include "CompletionManager.h"

#include <filesystem>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <fmt/core.h>

#ifdef _WIN32
#include <io.h>
#define access _access
#define X_OK 0  // Windows doesn't have X_OK, just check file existence
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace fs = std::filesystem;

CompletionManager::CompletionManager()
{
    fmt::print("CompletionManager: Initializing command cache...\n");
    
    // Scan PATH directories for executables
    scanPathDirectories();
    
    // Load shell builtin commands
    loadBuiltinCommands();
    
    fmt::print("CompletionManager: Cached {} commands\n", cachedCommands.size());
}

void CompletionManager::scanPathDirectories()
{
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) {
        fmt::print(stderr, "CompletionManager: PATH environment variable not set\n");
        return;
    }
    
    std::string pathStr(pathEnv);
    std::istringstream pathStream(pathStr);
    std::string directory;
    
    // PATH separator: ':' on Unix, ';' on Windows
#ifdef _WIN32
    const char pathSeparator = ';';
#else
    const char pathSeparator = ':';
#endif
    
    // Split PATH and scan each directory
    while (std::getline(pathStream, directory, pathSeparator)) {
        if (directory.empty()) continue;
        
        fs::path dirPath(directory);
        
        // Check if directory exists
        std::error_code ec;
        if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
            continue;
        }
        
        // Iterate over directory entries
        for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
            if (ec) break;
            
            const auto& path = entry.path();
            std::string filename = path.filename().string();
            
            // Skip hidden files (starting with '.')
            if (!filename.empty() && filename[0] == '.') {
                continue;
            }
            
            // Check if it's a regular file
            if (!entry.is_regular_file(ec)) {
                continue;
            }
            
            // Check if file is executable
            if (access(path.string().c_str(), X_OK) == 0) {
                // On Windows, also check for executable extensions
#ifdef _WIN32
                std::string ext = path.extension().string();
                // Convert to lowercase for comparison
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".exe" || ext == ".cmd" || ext == ".bat" || ext == ".com") {
                    // Store without extension for cleaner completion
                    cachedCommands.insert(path.stem().string());
                }
#else
                cachedCommands.insert(filename);
#endif
            }
        }
    }
    
    fmt::print("CompletionManager: Found {} executables in PATH\n", cachedCommands.size());
}

void CompletionManager::loadBuiltinCommands()
{
    // Try to get builtins from current shell
    // First try bash (compgen -b), then zsh (print builtins)
    
    FILE* pipe = nullptr;
    
    // Try bash first
    pipe = popen("bash -c 'compgen -b' 2>/dev/null", "r");
    
    if (!pipe) {
        // Try zsh
        pipe = popen("zsh -c 'print -l ${(k)builtins}' 2>/dev/null", "r");
    }
    
    if (pipe) {
        char buffer[256];
        size_t builtinCount = 0;
        
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // Remove trailing newline
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0';
            }
            
            // Skip empty lines
            if (buffer[0] != '\0') {
                cachedCommands.insert(buffer);
                builtinCount++;
            }
        }
        
        pclose(pipe);
        fmt::print("CompletionManager: Loaded {} shell builtins\n", builtinCount);
    } else {
        fmt::print(stderr, "CompletionManager: Could not load shell builtins\n");
    }
}

std::vector<std::string> CompletionManager::getCompletions(const std::string& text, int cursorPosition, const std::string& currentDir) const
{
    std::vector<std::string> completions;
    
    // If text is empty, return empty list (client will insert tab)
    if (text.empty()) {
        return completions;
    }
    
    // Analyze text to determine type of autocompletion
    std::string lastWord = extractLastWord(text, cursorPosition);
    
    // If last word is empty, return empty (insert tab)
    if (lastWord.empty()) {
        return completions;
    }
    
    if (isCommand(text, cursorPosition)) {
        // Command autocompletion
        completions = getCommandCompletions(lastWord);
    } else {
        // File and directory autocompletion
        completions = getFileCompletions(lastWord, currentDir);
    }
    
    fmt::print("CompletionManager: Found {} options for '{}' in directory '{}'\n", completions.size(), text, currentDir);
    return completions;
}

std::string CompletionManager::extractLastWord(const std::string& text, int cursorPosition) const
{
    if (text.empty() || cursorPosition <= 0) {
        return "";
    }
    
    // Find start of last word (search for space from right to left)
    int start = cursorPosition - 1;
    while (start >= 0 && text[start] != ' ' && text[start] != '\t') {
        start--;
    }
    start++; // Move to word start
    
    return text.substr(start, cursorPosition - start);
}

bool CompletionManager::isCommand(const std::string& text, int cursorPosition) const
{
    // Simple heuristic: if cursor is at line start or no spaces before cursor,
    // it's a command, otherwise - argument/file
    if (cursorPosition == 0) {
        return true;
    }
    
    // Search for spaces before cursor position
    for (int i = 0; i < cursorPosition && i < static_cast<int>(text.length()); i++) {
        if (text[i] == ' ' || text[i] == '\t') {
            return false; // Found space, so it's not a command
        }
    }
    
    return true; // No spaces found, it's a command
}

std::vector<std::string> CompletionManager::getCommandCompletions(const std::string& prefix) const
{
    std::vector<std::string> matches;
    
    // Search in cached commands (already sorted)
    for (const auto& cmd : cachedCommands) {
        // Use compare for efficiency - if cmd starts with prefix
        if (cmd.length() >= prefix.length() && 
            cmd.compare(0, prefix.length(), prefix) == 0) {
            matches.push_back(cmd);
        }
    }
    
    return matches;
}

std::vector<std::string> CompletionManager::getFileCompletions(const std::string& prefix, const std::string& currentDir) const
{
    std::vector<std::string> matches;
    
    // Determine directory to search in
    std::string searchDir = currentDir;
    std::string filePrefix = prefix;
    std::string originalDirPrefix = "";
    
    // Handle path separators (/ on Unix, \ or / on Windows)
#ifdef _WIN32
    size_t lastSlash = prefix.find_last_of("/\\");
#else
    size_t lastSlash = prefix.find_last_of('/');
#endif
    
    if (lastSlash != std::string::npos) {
        searchDir = prefix.substr(0, lastSlash);
        filePrefix = prefix.substr(lastSlash + 1);
        originalDirPrefix = searchDir + "/"; // Keep forward slash for consistency
        
        if (searchDir.empty()) {
#ifdef _WIN32
            searchDir = "."; // On Windows, empty means current dir
#else
            searchDir = "/"; // Root directory
#endif
            originalDirPrefix = searchDir == "." ? "" : "/";
        }
    } else {
        // If no slash, search in current directory
        searchDir = currentDir;
    }
    
    // Expand tilde to home directory for search
    std::string expandedSearchDir = expandTilde(searchDir);
    
    fs::path dirPath(expandedSearchDir);
    std::error_code ec;
    
    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) {
        return matches;
    }
    
    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (ec) break;
        
        std::string filename = entry.path().filename().string();
        
        // Skip hidden files (starting with dot)
        if (!filename.empty() && filename[0] == '.') {
            continue;
        }
        
        // Check match with prefix
        if (filename.length() >= filePrefix.length() &&
            filename.compare(0, filePrefix.length(), filePrefix) == 0) {
            // If it's a directory, add slash at end
            if (entry.is_directory(ec)) {
                filename += "/";
            }
            
            // Return full path with original prefix (including tilde)
            matches.push_back(originalDirPrefix + filename);
        }
    }
    
    return matches;
}

std::string CompletionManager::expandTilde(const std::string& path) const
{
    if (path.empty() || path[0] != '~') {
        return path; // No tilde, return as is
    }
    
    // Check if it's ~/... pattern
    if (path.length() == 1 || path[1] == '/' || path[1] == '\\') {
        // Get home directory
        const char* homeDir = nullptr;
        
#ifdef _WIN32
        // On Windows, try USERPROFILE first, then HOMEDRIVE+HOMEPATH
        homeDir = getenv("USERPROFILE");
        if (!homeDir) {
            const char* homeDrive = getenv("HOMEDRIVE");
            const char* homePath = getenv("HOMEPATH");
            if (homeDrive && homePath) {
                static std::string winHome;
                winHome = std::string(homeDrive) + homePath;
                homeDir = winHome.c_str();
            }
        }
#else
        // On Unix, try HOME, then passwd
        homeDir = getenv("HOME");
        if (!homeDir) {
            struct passwd* pw = getpwuid(getuid());
            if (pw) {
                homeDir = pw->pw_dir;
            }
        }
#endif
        
        if (homeDir) {
            if (path.length() == 1) {
                return std::string(homeDir); // Just ~
            } else {
                return std::string(homeDir) + path.substr(1); // ~/path
            }
        }
    }
#ifndef _WIN32
    else {
        // ~username/... - specific user's home directory (Unix only)
        size_t slashPos = path.find('/');
        std::string username = path.substr(1, slashPos - 1);
        
        struct passwd* pw = getpwnam(username.c_str());
        if (pw) {
            if (slashPos == std::string::npos) {
                return std::string(pw->pw_dir); // Just ~username
            } else {
                return std::string(pw->pw_dir) + path.substr(slashPos); // ~username/path
            }
        }
    }
#endif
    
    // If failed to expand tilde, return original path
    return path;
}
