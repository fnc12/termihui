#include "CompletionManager.h"

#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>

CompletionManager::CompletionManager()
{
    // Initialize list of popular commands
    commonCommands = {
        "ls", "cd", "pwd", "cat", "grep", "find", "mkdir", "rm", "cp", "mv",
        "chmod", "chown", "ps", "kill", "top", "df", "du", "free", "uname",
        "whoami", "which", "whereis", "locate", "man", "history", "clear",
        "echo", "touch", "head", "tail", "less", "more", "wc", "sort", "uniq",
        "tar", "gzip", "gunzip", "zip", "unzip", "wget", "curl", "ssh", "scp"
    };
}

std::vector<std::string> CompletionManager::getCompletions(const std::string& text, int cursorPosition, const std::string& currentDir) const
{
    std::vector<std::string> completions;
    
    // Simple autocompletion logic
    if (text.empty()) {
        // If text is empty, suggest popular commands
        completions = {"ls", "cd", "pwd", "cat", "grep", "find", "mkdir", "rm", "cp", "mv"};
    } else {
        // Analyze text to determine type of autocompletion
        std::string lastWord = extractLastWord(text, cursorPosition);
        
        if (isCommand(text, cursorPosition)) {
            // Command autocompletion
            completions = getCommandCompletions(lastWord);
        } else {
            // File and directory autocompletion
            completions = getFileCompletions(lastWord, currentDir);
        }
    }
    
    std::cout << "CompletionManager: Found " << completions.size() << " options for '" << text << "' in directory '" << currentDir << "'" << std::endl;
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
    // Simple heuristic: if cursor is at line start or after space/tab,
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
    
    for (const auto& cmd : commonCommands) {
        if (cmd.substr(0, prefix.length()) == prefix) {
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
    
    // If prefix contains slash, split path and filename
    size_t lastSlash = prefix.find_last_of('/');
    if (lastSlash != std::string::npos) {
        searchDir = prefix.substr(0, lastSlash);
        filePrefix = prefix.substr(lastSlash + 1);
        originalDirPrefix = searchDir + "/"; // Save original prefix with tilde
        
        if (searchDir.empty()) {
            searchDir = "/"; // Root directory
            originalDirPrefix = "/";
        }
    } else {
        // If no slash, search in current directory
        searchDir = currentDir;
    }
    
    // Expand tilde to home directory for search
    std::string expandedSearchDir = expandTilde(searchDir);
    
    std::cout << "CompletionManager: Searching files in '" << searchDir << "' (expanded: '" 
              << expandedSearchDir << "'), prefix: '" << filePrefix << "'" << std::endl;
    
    // Open directory
    DIR* dir = opendir(expandedSearchDir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            
            // Skip hidden files (starting with dot)
            if (filename[0] == '.') {
                continue;
            }
            
            // Check match with prefix
            if (filename.substr(0, filePrefix.length()) == filePrefix) {
                // If it's a directory, add slash at end
                std::string fullPath = expandedSearchDir + "/" + filename;
                struct stat statbuf;
                if (stat(fullPath.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                    filename += "/";
                }
                
                // Return full path with original prefix (including tilde)
                matches.push_back(originalDirPrefix + filename);
            }
        }
        closedir(dir);
        
        std::cout << "CompletionManager: Found " << matches.size() << " files" << std::endl;
    } else {
        std::cout << "CompletionManager: Failed to open directory '" << expandedSearchDir << "'" << std::endl;
    }
    
    return matches;
}

std::string CompletionManager::expandTilde(const std::string& path) const
{
    if (path.empty() || path[0] != '~') {
        return path; // No tilde, return as is
    }
    
    if (path.length() == 1 || path[1] == '/') {
        // ~/... - current user's home directory
        const char* homeDir = getenv("HOME");
        if (!homeDir) {
            // If HOME variable not set, get from passwd
            struct passwd* pw = getpwuid(getuid());
            if (pw) {
                homeDir = pw->pw_dir;
            }
        }
        
        if (homeDir) {
            if (path.length() == 1) {
                return std::string(homeDir); // Just ~
            } else {
                return std::string(homeDir) + path.substr(1); // ~/path
            }
        }
    } else {
        // ~username/... - specific user's home directory
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
    
    // If failed to expand tilde, return original path
    return path;
}
