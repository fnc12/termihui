#include "SessionManager.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <fmt/core.h>

SessionManager::SessionManager(int pollIntervalMs)
    : pollIntervalMs(pollIntervalMs)
{
}

SessionManager::~SessionManager()
{
    stop();
}

void SessionManager::start()
{
    if (this->running.exchange(true)) {
        return; // Уже запущен
    }
    
    this->pollThread = std::make_unique<std::thread>(&SessionManager::pollLoop, this);
    fmt::print("SessionManager запущен с интервалом {}мс\n", this->pollIntervalMs);
}

void SessionManager::stop()
{
    if (!this->running.exchange(false)) {
        return; // Уже остановлен
    }
    
    if (this->pollThread && this->pollThread->joinable()) {
        this->pollThread->join();
    }
    
    // Закрываем все сессии
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    for (auto& [sessionId, session] : this->sessions) {
        session->terminate();
    }
    this->sessions.clear();
    
    fmt::print("SessionManager остановлен\n");
}

bool SessionManager::createSession(const SessionId& sessionId, const std::string& command)
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    // Проверяем, не существует ли уже такая сессия
    if (this->sessions.find(sessionId) != this->sessions.end()) {
        fmt::print(stderr, "Сессия с ID '{}' уже существует\n", sessionId);
        return false;
    }
    
    // Создаем новую сессию
    auto session = std::make_shared<TerminalSession>();
    if (!session->startCommand(command)) {
        fmt::print(stderr, "Не удалось запустить команду '{}' для сессии '{}'\n", command, sessionId);
        return false;
    }
    
    this->sessions[sessionId] = session;
    
    // Обновляем статистику
    {
        std::lock_guard<std::mutex> statsLock(this->statsMutex);
        this->stats.totalSessions++;
        this->stats.activeSessions++;
    }
    
    fmt::print("Создана сессия '{}' (PID: {})\n", sessionId, session->getChildPid());
    return true;
}

bool SessionManager::closeSession(const SessionId& sessionId)
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    auto it = this->sessions.find(sessionId);
    if (it == this->sessions.end()) {
        return false;
    }
    
    // Завершаем сессию
    it->second->terminate();
    this->sessions.erase(it);
    
    // Обновляем статистику
    {
        std::lock_guard<std::mutex> statsLock(this->statsMutex);
        if (this->stats.activeSessions > 0) {
            this->stats.activeSessions--;
        }
    }
    
    fmt::print("Закрыта сессия '{}'\n", sessionId);
    return true;
}

ssize_t SessionManager::sendInput(const SessionId& sessionId, const std::string& input)
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    auto it = this->sessions.find(sessionId);
    if (it == this->sessions.end()) {
        return -1;
    }
    
    return it->second->sendInput(input);
}

bool SessionManager::hasSession(const SessionId& sessionId) const
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    return this->sessions.find(sessionId) != this->sessions.end();
}

std::vector<SessionManager::SessionId> SessionManager::getActiveSessions() const
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    std::vector<SessionId> result;
    result.reserve(this->sessions.size());
    
    for (const auto& [sessionId, session] : this->sessions) {
        if (session->isRunning()) {
            result.push_back(sessionId);
        }
    }
    
    return result;
}

void SessionManager::setOutputCallback(OutputCallback callback)
{
    this->outputCallback = std::move(callback);
}

void SessionManager::setStatusCallback(StatusCallback callback)
{
    this->statusCallback = std::move(callback);
}

SessionManager::Stats SessionManager::getStats() const
{
    std::lock_guard<std::mutex> lock(this->statsMutex);
    return this->stats;
}

void SessionManager::pollLoop()
{
    fmt::print("Запущен цикл опроса сессий\n");
    
    while (this->running.load()) {
        auto startTime = std::chrono::steady_clock::now();
        
        // Копируем список сессий для безопасного итерирования
        std::unordered_map<SessionId, std::shared_ptr<TerminalSession>> sessionsCopy;
        {
            std::lock_guard<std::mutex> lock(this->sessionsMutex);
            sessionsCopy = this->sessions;
        }
        
        // Опрашиваем все сессии
        for (const auto& [sessionId, session] : sessionsCopy) {
            pollSession(sessionId, session);
        }
        
        // Очищаем завершенные сессии
        cleanupFinishedSessions();
        
        // Обновляем статистику
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> statsLock(this->statsMutex);
            this->stats.pollCycles++;
            this->stats.avgPollTimeMs = (this->stats.avgPollTimeMs * (this->stats.pollCycles - 1) + duration.count() / 1000.0) / this->stats.pollCycles;
        }
        
        // Ждем до следующего цикла
        std::this_thread::sleep_for(std::chrono::milliseconds(this->pollIntervalMs));
    }
    
    fmt::print("Цикл опроса сессий завершен\n");
}

void SessionManager::pollSession(const SessionId& sessionId, std::shared_ptr<TerminalSession> session)
{
    if (!session->isRunning()) {
        return;
    }
    
    // Читаем вывод если он доступен
    if (session->hasData(0)) { // Неблокирующая проверка
        std::string output = session->readOutput();
        if (!output.empty() && this->outputCallback) {
            this->outputCallback(sessionId, output);
        }
    }
    
    // Проверяем изменение статуса
    static thread_local std::unordered_map<SessionId, bool> lastStatus;
    bool currentStatus = session->isRunning();
    
    if (lastStatus[sessionId] != currentStatus) {
        lastStatus[sessionId] = currentStatus;
        if (this->statusCallback) {
            this->statusCallback(sessionId, currentStatus);
        }
    }
}

void SessionManager::cleanupFinishedSessions()
{
    std::lock_guard<std::mutex> lock(this->sessionsMutex);
    
    auto it = this->sessions.begin();
    while (it != this->sessions.end()) {
        if (!it->second->isRunning()) {
            fmt::print("Удаляем завершенную сессию '{}'\n", it->first);
            
            // Обновляем статистику
            {
                std::lock_guard<std::mutex> statsLock(this->statsMutex);
                if (this->stats.activeSessions > 0) {
                    this->stats.activeSessions--;
                }
            }
            
            it = this->sessions.erase(it);
        } else {
            ++it;
        }
    }
} 