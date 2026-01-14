#include "TerminalSessionController.h"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fmt/core.h>
#include <fstream>

#ifdef __APPLE__
#include <util.h>
#elif __linux__
#include <pty.h>
#else
#error "Unsupported platform"
#endif

TerminalSessionController::TerminalSessionController(std::filesystem::path dbPath, uint64_t sessionId, uint64_t serverRunId, size_t bufferSize)
    : ptyFd(-1)
    , childPid(-1)
    , buffer(bufferSize)
    , bufferSize(bufferSize)
    , running(false)
    , sessionCreated(false)
    , prevRunningState(false)
    , sessionStorage(std::move(dbPath))
    , sessionId(sessionId)
    , serverRunId(serverRunId)
{
    this->sessionStorage.initialize();
}

TerminalSessionController::~TerminalSessionController()
{
    this->cleanup();
}

bool TerminalSessionController::createSession()
{
    if (this->sessionCreated) {
        fmt::print(stderr, "Session already created\n");
        return false;
    }

    // Create PTY
    this->childPid = forkpty(&this->ptyFd, nullptr, nullptr, nullptr);
    
    if (this->childPid < 0) {
        fmt::print(stderr, "forkpty error: {}\n", strerror(errno));
        return false;
    }
    
    if (this->childPid == 0) {
        // Child process
        // Set UTF-8 locale for proper Unicode handling
        setenv("LANG", "en_US.UTF-8", 1);
        setenv("LC_ALL", "en_US.UTF-8", 1);
        // Set empty prompt to remove "bash-3.2$"
        setenv("PS1", "", 1);
        // Suppress system banner about switching to zsh
        setenv("BASH_SILENCE_DEPRECATION_WARNING", "1", 1);
        // Set terminal type for TUI applications (nano, vim, htop, etc.)
        setenv("TERM", "xterm-256color", 1);

        // Disable local echo in slave TTY to avoid command duplication in output
        struct termios tio;
        if (tcgetattr(STDIN_FILENO, &tio) == 0) {
            tio.c_lflag &= ~ECHO; // keep canonical mode, remove only echo
            tcsetattr(STDIN_FILENO, TCSANOW, &tio);
        }

        // Create temporary rcfile with OSC 133 hooks (like Warp/iTerm2/VS Code)
        // Marker format:
        //  \033]133;A;cwd=<path>\007        — command start (preexec) with current directory
        //  \033]133;B;exit=<code>;cwd=<path>\007 — command end (precmd) with exit code and directory
        char rcTemplate[] = "/tmp/termihui_bashrc_XXXXXX";
        int rcFd = mkstemp(rcTemplate);
        if (rcFd >= 0) {
            std::string rcContent;
            rcContent.append("# TermiHUI shell integration (bash)\n");
            rcContent.append("export PS1=\"\"\n");
            // precmd: print OSC 133;B with previous command exit code and cwd
            rcContent.append("__termihui_precmd() { local ec=$?; printf '\\033]133;B;exit=%s;cwd=%s\\007' \"$ec\" \"$PWD\"; }\n");
            // wrapper for PROMPT_COMMAND: sets guard during precmd so DEBUG-trap doesn't trigger extra A
            rcContent.append("__termihui_precmd_wrapper() { local ec=$?; __TERMIHUI_IN_PRECMD=1; __termihui_precmd \"$ec\"; unset __TERMIHUI_IN_PRECMD; }\n");
            // preexec: print OSC 133;A before user command with current cwd, but NOT before PROMPT_COMMAND
            rcContent.append("__termihui_preexec() { if [[ -n \"$__TERMIHUI_IN_PRECMD\" ]]; then return; fi; if [[ \"$BASH_COMMAND\" == \"__termihui_precmd_wrapper\" || \"$BASH_COMMAND\" == \"__termihui_precmd\" ]]; then return; fi; printf '\\033]133;A;cwd=%s\\007' \"$PWD\"; }\n");
            // In bash use trap DEBUG as preexec equivalent
            rcContent.append("trap '__termihui_preexec' DEBUG\n");
            // PROMPT_COMMAND executes before printing prompt — precmd equivalent
            rcContent.append("PROMPT_COMMAND='__termihui_precmd_wrapper'\n");

            // Write file and close
            ssize_t ignored = write(rcFd, rcContent.data(), rcContent.size());
            (void)ignored;
            close(rcFd);

            // Start bash with our rcfile. --noprofile to avoid interference from other profiles
            execl("/bin/bash", "bash", "--noprofile", "--rcfile", rcTemplate, "-i", nullptr);
        }

        // Fallback: if rcfile creation failed — start regular bash
        execl("/bin/bash", "bash", "-i", nullptr);
        
        // If execl failed
        fmt::print(stderr, "execl error: {}\n", strerror(errno));
        _exit(1);
    }
    
    // Parent process
    this->setupNonBlocking();
    this->running = true;
    this->sessionCreated = true;
    
    fmt::print("Interactive bash session created (PID: {})\n", this->childPid);
    
    return true;
}

TerminalSessionController::ExecuteCommandResult TerminalSessionController::executeCommand(std::string_view command)
{
    if (!this->sessionCreated || !this->running) {
        return {SessionNotCreatedOrInactiveError{}};
    }
    
    // Send command + newline to interactive shell
    std::string cmd = std::string(command) + "\n";
    const ssize_t bytesWritten = write(this->ptyFd, cmd.c_str(), cmd.length());
    
    if (bytesWritten < 0) {
        std::string errorText = fmt::format("Command send error: {}", strerror(errno));
        return {CommandSendError{std::move(errorText)}};
    }
    
    return {ExecuteCommandSuccess{bytesWritten}};
}

ssize_t TerminalSessionController::sendInput(std::string_view input)
{
    if (!this->running || this->ptyFd < 0) {
        return -1;
    }
    
    ssize_t bytesWritten = write(this->ptyFd, input.data(), input.length());
    if (bytesWritten < 0) {
        fmt::print(stderr, "PTY write error: {}\n", strerror(errno));
    }
    
    return bytesWritten;
}

std::string TerminalSessionController::readOutput()
{
    if (!this->running || this->ptyFd < 0) {
        return "";
    }
    
    std::string output;
    
    // Read all available data
    while (this->hasData()) {
        ssize_t bytesRead = read(this->ptyFd, this->buffer.data(), this->bufferSize);
        
        if (bytesRead > 0) {
            output.append(this->buffer.data(), bytesRead);
        } else if (bytesRead == 0) {
            // EOF - process terminated
            this->checkChildStatus();
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data to read
                break;
            } else {
                fmt::print(stderr, "PTY read error: {}\n", strerror(errno));
                break;
            }
        }
    }
    
    // Check child process status
    this->checkChildStatus();
    
    return output;
}

bool TerminalSessionController::isRunning() const
{
    return this->running;
}

bool TerminalSessionController::didJustFinishRunning()
{
    bool currentlyRunning = this->running;
    bool justFinished = this->prevRunningState && !currentlyRunning;
    this->prevRunningState = currentlyRunning;
    return justFinished;
}

pid_t TerminalSessionController::getChildPid() const
{
    return this->childPid;
}

void TerminalSessionController::terminate()
{
    if (this->running && this->childPid > 0) {
        // First try graceful termination
        kill(this->childPid, SIGTERM);
        
        // Wait a bit
        usleep(100000); // 100ms
        
        // If process still running, force terminate
        if (this->running) {
            kill(this->childPid, SIGKILL);
        }
    }
    
    this->cleanup();
}

int TerminalSessionController::getPtyFd() const
{
    return this->ptyFd;
}

bool TerminalSessionController::hasData() const
{
    if (this->ptyFd < 0) {
        return false;
    }
    
    struct pollfd pfd;
    pfd.fd = this->ptyFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    
    int result = poll(&pfd, 1, 0);  // Non-blocking
    
    if (result > 0) {
        return (pfd.revents & POLLIN) != 0;
    }
    
    return false;
}

void TerminalSessionController::setupNonBlocking()
{
    if (this->ptyFd >= 0) {
        int flags = fcntl(this->ptyFd, F_GETFL);
        if (flags >= 0) {
            fcntl(this->ptyFd, F_SETFL, flags | O_NONBLOCK);
        }
    }
}

void TerminalSessionController::cleanup()
{
    if (this->ptyFd >= 0) {
        close(this->ptyFd);
        this->ptyFd = -1;
    }
    
    if (this->childPid > 0) {
        // Wait for child process termination
        int status;
        waitpid(this->childPid, &status, WNOHANG);
        this->childPid = -1;
    }
    
    this->running = false;
    this->sessionCreated = false;
}

void TerminalSessionController::checkChildStatus()
{
    if (this->childPid <= 0) {
        return;
    }
    
    int status;
    pid_t result = waitpid(this->childPid, &status, WNOHANG);
    
    if (result > 0) {
        // Child process terminated
        this->running = false;
        this->childPid = -1;
        
        // TODO: In the future can add callback for termination handling
        // if (this->onProcessExit) {
        //     this->onProcessExit(status, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        // }
    } else if (result < 0 && errno != ECHILD) {
        fmt::print(stderr, "waitpid error: {}\n", strerror(errno));
    }
}

void TerminalSessionController::setLastKnownCwd(const std::string& cwd)
{
    if (!cwd.empty()) {
        lastKnownCwd = cwd;
        fmt::print("Updated lastKnownCwd: '{}'\n", lastKnownCwd);
    }
}

std::string TerminalSessionController::getLastKnownCwd() const
{
    return lastKnownCwd;
}

std::string TerminalSessionController::getCurrentWorkingDirectory() const
{
    if (childPid <= 0) {
        return "";
    }
    
#ifdef __APPLE__
    // On macOS need to find actual bash process which may be a child process
    // First try to find bash process among child processes
    std::string findBashCommand = "pgrep -P " + std::to_string(childPid) + " bash 2>/dev/null | head -1";
    
    fmt::print("Searching for bash process: {}\n", findBashCommand);
    
    FILE* bashPipe = popen(findBashCommand.c_str(), "r");
    pid_t bashPid = childPid; // Default to original PID
    
    if (bashPipe) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), bashPipe) != nullptr) {
            bashPid = std::atoi(buffer);
            fmt::print("Found bash process with PID: {}\n", bashPid);
        }
        pclose(bashPipe);
    }
    
    // Now get cwd for found bash process
    std::string command = "lsof -p " + std::to_string(bashPid) + " -d cwd -Fn 2>/dev/null | grep '^n' | cut -c2-";
    
    fmt::print("Executing command: {}\n", command);
    
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[PATH_MAX];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            pclose(pipe);
            // Remove trailing newline
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            fmt::print("lsof returned: '{}'\n", result);
            if (!result.empty() && result != "/") {
                return result;
            }
        }
        pclose(pipe);
    }
    
    fmt::print("lsof failed, using fallback directory\n");
    
    // Fallback - use user's home directory
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home);
    }
    
    // Last fallback - server's current directory
    return ".";
    
#else
    // On Linux use /proc/{pid}/cwd
    std::string procPath = "/proc/" + std::to_string(childPid) + "/cwd";
    
    char buffer[PATH_MAX];
    ssize_t len = readlink(procPath.c_str(), buffer, sizeof(buffer) - 1);
    
    if (len != -1) {
        buffer[len] = '\0';
        return std::string(buffer);
    }
#endif
    
    fmt::print(stderr, "Failed to get current directory for process {}\n", childPid);
    return "";
}

bool TerminalSessionController::setWindowSize(unsigned short cols, unsigned short rows)
{
    if (this->ptyFd < 0) {
        fmt::print(stderr, "Cannot set window size: PTY not initialized\n");
        return false;
    }
    
    struct winsize ws;
    ws.ws_col = cols;
    ws.ws_row = rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    
    if (ioctl(this->ptyFd, TIOCSWINSZ, &ws) < 0) {
        fmt::print(stderr, "Failed to set window size: {}\n", strerror(errno));
        return false;
    }
    
    fmt::print("Terminal size set to {}x{}\n", cols, rows);
    return true;
}

void TerminalSessionController::setPendingCommand(std::string command) {
    this->pendingCommand = std::move(command);
}

void TerminalSessionController::startCommandInHistory(const std::string& cwd) {
    if (this->pendingCommand.empty()) {
        return;
    }
    
    this->currentCommandId = this->sessionStorage.addCommand(this->serverRunId, this->pendingCommand, cwd);
    fmt::print("Added command to storage with ID: {}\n", this->currentCommandId);
    
    this->pendingCommand.clear();
}

void TerminalSessionController::appendOutputToCurrentCommand(const std::string& output) {
    if (this->currentCommandId > 0) {
        this->sessionStorage.appendOutput(this->currentCommandId, output);
    } else {
        fmt::print("[WARN] appendOutputToCurrentCommand called with no active command, output ignored ({} bytes)\n", output.size());
    }
}

void TerminalSessionController::finishCurrentCommand(int exitCode, const std::string& cwd) {
    if (this->currentCommandId > 0) {
        this->sessionStorage.finishCommand(this->currentCommandId, exitCode, cwd);
        fmt::print("Finished command ID: {} with exit code: {}\n", this->currentCommandId, exitCode);
        this->currentCommandId = 0;
    } else {
        fmt::print("[WARN] finishCurrentCommand called with no active command (exit={}, cwd={})\n", exitCode, cwd);
    }
}

std::vector<SessionCommand> TerminalSessionController::getCommandHistory() {
    return this->sessionStorage.getAllCommands();
}

bool TerminalSessionController::hasActiveCommand() const {
    return this->currentCommandId > 0;
}

