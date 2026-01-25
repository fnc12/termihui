#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <sys/types.h>
#include <variant>

#include "SessionStorage.h"
#include "VirtualScreen.h"
#include "AnsiProcessor.h"

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
     * @param dbPath path to session storage database
     * @param sessionId unique session ID (from database)
     * @param serverRunId current server run ID
     * @param bufferSize output buffer size (default 4096)
     */
    TerminalSessionController(std::filesystem::path dbPath, uint64_t sessionId, uint64_t serverRunId, size_t bufferSize = 4096);
    
    /**
     * Get session ID
     */
    uint64_t getSessionId() const { return this->sessionId; }
    
    /**
     * Virtual destructor - automatically terminates session
     */
    virtual ~TerminalSessionController();
    
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
    virtual std::string readOutput();
    
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
    virtual bool hasData() const;
    
    /**
     * Get current working directory of bash process
     * @return path to current directory or empty string on error
     */
    std::string getCurrentWorkingDirectory() const;
    
    /**
     * Set last known cwd (from OSC markers)
     * @param cwd path to directory
     */
    virtual void setLastKnownCwd(const std::string& cwd);
    
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
    
    /**
     * Set pending command (before execution)
     * @param command command text
     */
    virtual void setPendingCommand(std::string command);
    
    /**
     * Start new command in history (called on OSC 133;A)
     * @param cwd current working directory
     */
    virtual void startCommandInHistory(const std::string& cwd);
    
    /**
     * Append output to current command in history
     * @param output output text to append
     */
    virtual void appendOutputToCurrentCommand(const std::string& output);
    
    /**
     * Finish current command in history (called on OSC 133;B)
     * @param exitCode command exit code
     * @param cwd final working directory
     */
    virtual void finishCurrentCommand(int exitCode, const std::string& cwd);
    
    /**
     * Get command history for current server run
     * @return vector of commands from storage
     */
    std::vector<SessionCommand> getCommandHistory();
    
    /**
     * Check if there's an active command being recorded
     * @return true if a command is currently being recorded
     */
    virtual bool hasActiveCommand() const;
    
    /**
     * Check if in interactive mode (alternate screen buffer)
     */
    bool isInInteractiveMode() const { return this->interactiveMode; }
    
    /**
     * Set interactive mode state
     * @param enabled true to enable, false to disable
     */
    void setInteractiveMode(bool enabled) { 
        if (this->interactiveMode && !enabled) {
            // Exiting interactive mode - set flag to skip output until command_end
            this->justExitedInteractiveMode = true;
        }
        this->interactiveMode = enabled; 
    }
    
    /**
     * Check if we just exited interactive mode (need to skip output recording)
     */
    bool hasJustExitedInteractiveMode() const { return this->justExitedInteractiveMode; }
    
    /**
     * Clear the just exited interactive mode flag (called after command_end)
     */
    void clearJustExitedInteractiveMode() { this->justExitedInteractiveMode = false; }
    
    /**
     * Get virtual screen reference
     */
    termihui::VirtualScreen& getVirtualScreen() { return this->virtualScreen; }
    const termihui::VirtualScreen& getVirtualScreen() const { return this->virtualScreen; }
    
    /**
     * Get ANSI processor reference
     */
    termihui::AnsiProcessor& getAnsiProcessor() { return this->ansiProcessor; }

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
    
    // Session storage for persistent command history
    SessionStorage sessionStorage;
    uint64_t sessionId;
    uint64_t serverRunId;
    uint64_t currentCommandId = 0;  // ID of active command in storage
    std::string pendingCommand;
    
    // Virtual screen for terminal emulation
    termihui::VirtualScreen virtualScreen;
    termihui::AnsiProcessor ansiProcessor;
    bool interactiveMode = false;
    bool justExitedInteractiveMode = false;  // Flag to skip output recording after exiting interactive mode
    
    // TODO: Add in future:
    // - struct winsize m_windowSize;  // Terminal window size for resize events
    // - std::string m_lastCommand;    // Last executed command
    // - std::queue<std::string> m_commandQueue; // Command queue for chain
    // - bool m_sudoMode;              // Sudo mode for privileged operations
    // - std::function<void(const std::string&)> m_aiAnalyzer; // Callback for AI analysis
    // - std::chrono::steady_clock::time_point m_lastActivity; // Time of last activity
    // - std::string m_promptPattern;  // Pattern for detecting command completion
}; 
