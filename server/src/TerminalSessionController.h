#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <sys/types.h>
#include <variant>

#include "CompletionManager.h"

/**
 * Class for managing terminal sessions via PTY
 * 
 * Features:
 * - Asynchronous command execution via forkpty
 * - Non-blocking output reading
 * - Data buffering
 * - Process state checking
 */
class TerminalSessionController {
public:
    /**
     * Constructor
     * @param bufferSize output buffer size (default 4096)
     */
    explicit TerminalSessionController(size_t bufferSize = 4096);
    
    /**
     * Destructor - automatically terminates session
     */
    ~TerminalSessionController();
    
    // Disable copying and moving
    TerminalSessionController(const TerminalSessionController&) = delete;
    TerminalSessionController& operator=(const TerminalSessionController&) = delete;
    TerminalSessionController(TerminalSessionController&&) = delete;
    TerminalSessionController& operator=(TerminalSessionController&&) = delete;
    
    /**
     * Create interactive bash session
     * @return true if session successfully created
     */
    bool createSession();
    
    struct SessionNotCreatedOrInactiveError {};

    struct CommandSendError {
        std::string errorText;
    };

    struct ExecuteCommandSuccess {
        ssize_t bytesWritten = 0;
    };
    
    struct ExecuteCommandResult {
        std::variant<ExecuteCommandSuccess, CommandSendError, SessionNotCreatedOrInactiveError> data;
        
        bool isOk() const {
            return std::holds_alternative<ExecuteCommandSuccess>(this->data);
        }
        
        std::string errorText() const {
            if (this->isOk()) {
                return {};
            }
            return std::visit([this] (auto &value) {
                return this->errorTextFor(value);
            }, this->data);
        }
        
        std::string errorTextFor(const CommandSendError &commandSendError) const {
            return commandSendError.errorText;
        }
        
        std::string errorTextFor([[maybe_unused]] const ExecuteCommandSuccess &executeCommandSuccess) const {
            return {};
        }
        
        std::string errorTextFor([[maybe_unused]] const SessionNotCreatedOrInactiveError &sessionNotCreatedOrInactiveError) const {
            return "Session not created or inactive";
        }
    };
    
    /**
     * Execute command in existing session
     * @param command command to execute
     * @return true if command successfully sent
     */
    ExecuteCommandResult executeCommand(std::string_view command);
    
    /**
     * Send text to PTY
     * @param input text to send
     * @return number of bytes sent, -1 on error
     */
    ssize_t sendInput(std::string_view input);
    
    /**
     * Read available output from PTY
     * @return string with new output
     */
    std::string readOutput();
    
    /**
     * Check if process is running
     * @return true if process is active
     */
    bool isRunning() const;
    
    /**
     * Check if session just finished running (transitioned from running to not running)
     * Updates internal state tracking
     * @return true if session just finished
     */
    bool didJustFinishRunning();
    
    /**
     * Get child process PID
     * @return process PID or -1 if not running
     */
    pid_t getChildPid() const;
    
    /**
     * Force terminate session
     */
    void terminate();
    
    /**
     * Get PTY file descriptor (for external polling)
     * @return file descriptor or -1
     */
    int getPtyFd() const;
    
    /**
     * Check if data is available for reading (non-blocking)
     * @return true if data is available
     */
    bool hasData() const;
    
    /**
     * Get autocompletion options for text
     * @param text text to autocomplete
     * @param cursorPosition cursor position in text
     * @return vector of strings with completion options
     */
    std::vector<std::string> getCompletions(const std::string& text, int cursorPosition) const;
    
    /**
     * Get current working directory of bash process
     * @return path to current directory or empty string on error
     */
    std::string getCurrentWorkingDirectory() const;
    
    /**
     * Set last known cwd (from OSC markers)
     * @param cwd path to directory
     */
    void setLastKnownCwd(const std::string& cwd);
    
    /**
     * Get last known cwd
     * @return path or empty string
     */
    std::string getLastKnownCwd() const;
    
    /**
     * Set terminal window size
     * @param cols number of columns (width in characters)
     * @param rows number of rows (height in characters)
     * @return true if size was set successfully
     */
    bool setWindowSize(unsigned short cols, unsigned short rows);

private:
    /**
     * Setup PTY in non-blocking mode
     */
    void setupNonBlocking();
    
    /**
     * Resource cleanup
     */
    void cleanup();
    
    /**
     * Check child process status
     */
    void checkChildStatus();

private:
    int ptyFd;                    // PTY file descriptor
    pid_t childPid;               // Child process PID
    std::vector<char> buffer;     // Buffer for reading data
    size_t bufferSize;            // Buffer size
    bool running;                 // Process activity flag
    bool sessionCreated;          // Session created flag
    bool prevRunningState;        // Previous running state for transition detection
    
    // Last known cwd from OSC markers
    mutable std::string lastKnownCwd;
    
    // Completion manager
    CompletionManager completionManager;
    
    // TODO: Add in future:
    // - struct winsize m_windowSize;  // Terminal window size for resize events
    // - std::string m_lastCommand;    // Last executed command
    // - std::queue<std::string> m_commandQueue; // Command queue for chain
    // - bool m_sudoMode;              // Sudo mode for privileged operations
    // - std::function<void(const std::string&)> m_aiAnalyzer; // Callback for AI analysis
    // - std::chrono::steady_clock::time_point m_lastActivity; // Time of last activity
    // - std::string m_promptPattern;  // Pattern for detecting command completion
}; 
