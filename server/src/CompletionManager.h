#pragma once

#include <string>
#include <vector>
#include <set>

/**
 * Autocompletion manager for terminal commands and files
 * 
 * Features:
 * - Command autocompletion from PATH executables
 * - Shell builtin commands support
 * - File and directory autocompletion
 * - Tilde (~) support for home directories
 * - Context detection (command vs file)
 */
class CompletionManager {
public:
    /**
     * Constructor - initializes command cache from PATH and builtins
     */
    CompletionManager();
    
    /**
     * Destructor
     */
    ~CompletionManager() = default;
    
    // Disable copying
    CompletionManager(const CompletionManager&) = delete;
    CompletionManager& operator=(const CompletionManager&) = delete;
    
    /**
     * Get autocompletion options for text
     * @param text full command text
     * @param cursorPosition cursor position in text
     * @param currentDir current working directory for file search
     * @return vector of strings with completion options
     */
    std::vector<std::string> getCompletions(const std::string& text, int cursorPosition, const std::string& currentDir = ".") const;
    
    /**
     * Get number of cached commands
     */
    size_t getCachedCommandCount() const { return cachedCommands.size(); }
    
private:
    /**
     * Scan PATH directories and cache all executable commands
     */
    void scanPathDirectories();
    
    /**
     * Load shell builtin commands
     */
    void loadBuiltinCommands();
    
    /**
     * Extract last word from text
     * @param text text to analyze
     * @param cursorPosition cursor position
     * @return last word before cursor
     */
    std::string extractLastWord(const std::string& text, int cursorPosition) const;
    
    /**
     * Check if current position is a command
     * @param text full text
     * @param cursorPosition cursor position
     * @return true if it's a command, false if argument/file
     */
    bool isCommand(const std::string& text, int cursorPosition) const;
    
    /**
     * Get command autocompletion
     * @param prefix command prefix
     * @return list of matching commands
     */
    std::vector<std::string> getCommandCompletions(const std::string& prefix) const;
    
    /**
     * Get file and directory autocompletion
     * @param prefix path prefix
     * @param currentDir current working directory
     * @return list of matching files/directories
     */
    std::vector<std::string> getFileCompletions(const std::string& prefix, const std::string& currentDir = ".") const;
    
    /**
     * Expand tilde (~) to full home directory path
     * @param path path with possible tilde
     * @return path with expanded tilde
     */
    std::string expandTilde(const std::string& path) const;
    
private:
    // Cached commands from PATH + builtins (sorted, no duplicates)
    std::set<std::string> cachedCommands;
};
