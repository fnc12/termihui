#include "TerminalSession.h"

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

TerminalSession::TerminalSession(size_t bufferSize)
    : ptyFd(-1)
    , childPid(-1)
    , buffer(bufferSize)
    , bufferSize(bufferSize)
    , running(false)
    , sessionCreated(false)
{
}

TerminalSession::~TerminalSession()
{
    this->cleanup();
}

TerminalSession::TerminalSession(TerminalSession&& other) noexcept
    : ptyFd(other.ptyFd)
    , childPid(other.childPid)
    , buffer(std::move(other.buffer))
    , bufferSize(other.bufferSize)
    , running(other.running)
    , sessionCreated(other.sessionCreated)
{
    other.ptyFd = -1;
    other.childPid = -1;
    other.running = false;
    other.sessionCreated = false;
}

TerminalSession& TerminalSession::operator=(TerminalSession&& other) noexcept
{
    if (this != &other) {
        this->cleanup();
        
        this->ptyFd = other.ptyFd;
        this->childPid = other.childPid;
        this->buffer = std::move(other.buffer);
        this->bufferSize = other.bufferSize;
        this->running = other.running;
        this->sessionCreated = other.sessionCreated;
        
        other.ptyFd = -1;
        other.childPid = -1;
        other.running = false;
        other.sessionCreated = false;
    }
    return *this;
}

bool TerminalSession::createSession()
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
        // Set empty prompt to remove "bash-3.2$"
        setenv("PS1", "", 1);
        // Suppress system banner about switching to zsh
        setenv("BASH_SILENCE_DEPRECATION_WARNING", "1", 1);

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

bool TerminalSession::executeCommand(const std::string& command)
{
    if (!this->sessionCreated || !this->running) {
        fmt::print(stderr, "Session not created or inactive\n");
        return false;
    }
    
    // Send command + newline to interactive bash
    std::string cmd = command + "\n";
    ssize_t bytesWritten = write(this->ptyFd, cmd.c_str(), cmd.length());
    
    if (bytesWritten < 0) {
        fmt::print(stderr, "Command send error: {}\n", strerror(errno));
        return false;
    }
    
    fmt::print("Command sent: {}\n", command);
    return true;
}

ssize_t TerminalSession::sendInput(std::string_view input)
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

std::string TerminalSession::readOutput()
{
    if (!this->running || this->ptyFd < 0) {
        return "";
    }
    
    std::string output;
    
    // Read all available data
    while (this->hasData(0)) {
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

bool TerminalSession::isRunning() const
{
    return this->running;
}

pid_t TerminalSession::getChildPid() const
{
    return this->childPid;
}

void TerminalSession::terminate()
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

int TerminalSession::getPtyFd() const
{
    return this->ptyFd;
}

bool TerminalSession::hasData(int timeoutMs) const
{
    if (this->ptyFd < 0) {
        return false;
    }
    
    struct pollfd pfd;
    pfd.fd = this->ptyFd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    
    int result = poll(&pfd, 1, timeoutMs);
    
    if (result > 0) {
        return (pfd.revents & POLLIN) != 0;
    }
    
    return false;
}

void TerminalSession::setupNonBlocking()
{
    if (this->ptyFd >= 0) {
        int flags = fcntl(this->ptyFd, F_GETFL);
        if (flags >= 0) {
            fcntl(this->ptyFd, F_SETFL, flags | O_NONBLOCK);
        }
    }
}

void TerminalSession::cleanup()
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

void TerminalSession::checkChildStatus()
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

std::vector<std::string> TerminalSession::getCompletions(const std::string& text, int cursorPosition) const
{
    // First try using lastKnownCwd from OSC markers (most reliable)
    std::string currentDir = lastKnownCwd;
    
    // If lastKnownCwd is empty — fallback to lsof
    if (currentDir.empty()) {
        currentDir = getCurrentWorkingDirectory();
    }
    
    if (currentDir.empty()) {
        fmt::print(stderr, "Failed to get current directory, using '.'\n");
        currentDir = ".";
    }
    
    fmt::print("Current bash working directory: '{}'\n", currentDir);
    
    // Delegate to CompletionManager with correct directory
    return completionManager.getCompletions(text, cursorPosition, currentDir);
}

void TerminalSession::setLastKnownCwd(const std::string& cwd)
{
    if (!cwd.empty()) {
        lastKnownCwd = cwd;
        fmt::print("Updated lastKnownCwd: '{}'\n", lastKnownCwd);
    }
}

std::string TerminalSession::getLastKnownCwd() const
{
    return lastKnownCwd;
}

std::string TerminalSession::getCurrentWorkingDirectory() const
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



 