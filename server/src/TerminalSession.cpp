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
        fmt::print(stderr, "Сессия уже создана\n");
        return false;
    }

    // Создаем PTY
    this->childPid = forkpty(&this->ptyFd, nullptr, nullptr, nullptr);
    
    if (this->childPid < 0) {
        fmt::print(stderr, "Ошибка forkpty: {}\n", strerror(errno));
        return false;
    }
    
    if (this->childPid == 0) {
        // Дочерний процесс
        // Устанавливаем пустой промпт чтобы убрать "bash-3.2$"
        setenv("PS1", "", 1);
        
        // Запускаем интерактивный bash
        execl("/bin/bash", "bash", "-i", nullptr);
        
        // Если execl не сработал
        fmt::print(stderr, "Ошибка execl: {}\n", strerror(errno));
        _exit(1);
    }
    
    // Родительский процесс
    this->setupNonBlocking();
    this->running = true;
    this->sessionCreated = true;
    
    fmt::print("Интерактивная bash-сессия создана (PID: {})\n", this->childPid);
    
    return true;
}

bool TerminalSession::executeCommand(const std::string& command)
{
    if (!this->sessionCreated || !this->running) {
        fmt::print(stderr, "Сессия не создана или не активна\n");
        return false;
    }
    
    // Отправляем команду + перенос строки в интерактивный bash
    std::string cmd = command + "\n";
    ssize_t bytesWritten = write(this->ptyFd, cmd.c_str(), cmd.length());
    
    if (bytesWritten < 0) {
        fmt::print(stderr, "Ошибка отправки команды: {}\n", strerror(errno));
        return false;
    }
    
    fmt::print("Команда отправлена: {}\n", command);
    return true;
}

ssize_t TerminalSession::sendInput(std::string_view input)
{
    if (!this->running || this->ptyFd < 0) {
        return -1;
    }
    
    ssize_t bytesWritten = write(this->ptyFd, input.data(), input.length());
    if (bytesWritten < 0) {
        fmt::print(stderr, "Ошибка записи в PTY: {}\n", strerror(errno));
    }
    
    return bytesWritten;
}

std::string TerminalSession::readOutput()
{
    if (!this->running || this->ptyFd < 0) {
        return "";
    }
    
    std::string output;
    
    // Читаем все доступные данные
    while (this->hasData(0)) {
        ssize_t bytesRead = read(this->ptyFd, this->buffer.data(), this->bufferSize);
        
        if (bytesRead > 0) {
            output.append(this->buffer.data(), bytesRead);
        } else if (bytesRead == 0) {
            // EOF - процесс завершился
            this->checkChildStatus();
            break;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Нет данных для чтения
                break;
            } else {
                fmt::print(stderr, "Ошибка чтения из PTY: {}\n", strerror(errno));
                break;
            }
        }
    }
    
    // Проверяем статус дочернего процесса
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
        // Сначала пробуем мягкое завершение
        kill(this->childPid, SIGTERM);
        
        // Ждем немного
        usleep(100000); // 100ms
        
        // Если процесс еще работает, принудительно завершаем
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
        // Ожидаем завершения дочернего процесса
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
        // Дочерний процесс завершился
        this->running = false;
        this->childPid = -1;
        
        // TODO: В будущем можно добавить коллбэк для обработки завершения
        // if (this->onProcessExit) {
        //     this->onProcessExit(status, WIFEXITED(status) ? WEXITSTATUS(status) : -1);
        // }
    } else if (result < 0 && errno != ECHILD) {
        fmt::print(stderr, "Ошибка waitpid: {}\n", strerror(errno));
    }
}

std::vector<std::string> TerminalSession::getCompletions(const std::string& text, int cursorPosition) const
{
    // Получаем текущую рабочую директорию bash-процесса
    std::string currentDir = getCurrentWorkingDirectory();
    if (currentDir.empty()) {
        fmt::print(stderr, "Не удалось получить текущую директорию, используем '.'\n");
        currentDir = ".";
    }
    
    fmt::print("Текущая рабочая директория bash: '{}'\n", currentDir);
    
    // Делегируем работу CompletionManager с правильной директорией
    return completionManager.getCompletions(text, cursorPosition, currentDir);
}

std::string TerminalSession::getCurrentWorkingDirectory() const
{
    if (childPid <= 0) {
        return "";
    }
    
#ifdef __APPLE__
    // На macOS нужно найти реальный bash-процесс, который может быть дочерним процессом
    // Сначала пробуем найти bash-процесс среди дочерних процессов
    std::string findBashCommand = "pgrep -P " + std::to_string(childPid) + " bash 2>/dev/null | head -1";
    
    fmt::print("Ищем bash-процесс: {}\n", findBashCommand);
    
    FILE* bashPipe = popen(findBashCommand.c_str(), "r");
    pid_t bashPid = childPid; // По умолчанию используем исходный PID
    
    if (bashPipe) {
        char buffer[32];
        if (fgets(buffer, sizeof(buffer), bashPipe) != nullptr) {
            bashPid = std::atoi(buffer);
            fmt::print("Найден bash-процесс с PID: {}\n", bashPid);
        }
        pclose(bashPipe);
    }
    
    // Теперь получаем cwd для найденного bash-процесса
    std::string command = "lsof -p " + std::to_string(bashPid) + " -d cwd -Fn 2>/dev/null | grep '^n' | cut -c2-";
    
    fmt::print("Выполняем команду: {}\n", command);
    
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe) {
        char buffer[PATH_MAX];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            pclose(pipe);
            // Удаляем символ новой строки в конце
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            fmt::print("lsof вернул: '{}'\n", result);
            if (!result.empty() && result != "/") {
                return result;
            }
        }
        pclose(pipe);
    }
    
    fmt::print("lsof не сработал, используем fallback директорию\n");
    
    // Fallback - используем домашнюю директорию пользователя
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home);
    }
    
    // Последний fallback - текущая директория сервера
    return ".";
    
#else
    // На Linux используем /proc/{pid}/cwd
    std::string procPath = "/proc/" + std::to_string(childPid) + "/cwd";
    
    char buffer[PATH_MAX];
    ssize_t len = readlink(procPath.c_str(), buffer, sizeof(buffer) - 1);
    
    if (len != -1) {
        buffer[len] = '\0';
        return std::string(buffer);
    }
#endif
    
    fmt::print(stderr, "Не удалось получить текущую директорию процесса {}\n", childPid);
    return "";
}



 