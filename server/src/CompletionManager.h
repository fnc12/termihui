#pragma once

#include <string>
#include <vector>

/**
 * Autocompletion manager for terminal commands and files
 * 
 * Features:
 * - Command autocompletion from popular commands list
 * - File and directory autocompletion
 * - Tilde (~) support for home directories
 * - Context detection (command vs file)
 */
class CompletionManager {
public:
    /**
     * Constructor
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
    
private:
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
    // List of popular commands for autocompletion
    std::vector<std::string> commonCommands;
};
