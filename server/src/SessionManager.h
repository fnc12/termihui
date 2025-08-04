#pragma once

#include "TerminalSession.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <vector>
#include <chrono>

/**
 * Менеджер терминальных сессий
 * 
 * Особенности:
 * - Управление множественными сессиями (вкладками)
 * - Потокобезопасные операции
 * - Неблокирующее чтение всех сессий
 * - Автоматическая очистка завершенных сессий
 */
class SessionManager {
public:
    using SessionId = std::string;
    using OutputCallback = std::function<void(const SessionId&, const std::string&)>;
    using StatusCallback = std::function<void(const SessionId&, bool isRunning)>;
    
    /**
     * Конструктор
     * @param pollIntervalMs интервал опроса сессий в миллисекундах
     */
    explicit SessionManager(int pollIntervalMs = 10);
    
    /**
     * Деструктор
     */
    ~SessionManager();
    
    // Запрещаем копирование
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    
    /**
     * Запуск менеджера (начинает фоновый поток)
     */
    void start();
    
    /**
     * Остановка менеджера
     */
    void stop();
    
    /**
     * Создание новой сессии
     * @param sessionId уникальный идентификатор сессии
     * @param command команда для выполнения (по умолчанию bash)
     * @return true если сессия создана успешно
     */
    bool createSession(const SessionId& sessionId, const std::string& command = "bash");
    
    /**
     * Закрытие сессии
     * @param sessionId идентификатор сессии
     * @return true если сессия была найдена и закрыта
     */
    bool closeSession(const SessionId& sessionId);
    
    /**
     * Отправка ввода в сессию
     * @param sessionId идентификатор сессии
     * @param input данные для отправки
     * @return количество отправленных байт, -1 при ошибке
     */
    ssize_t sendInput(const SessionId& sessionId, const std::string& input);
    
    /**
     * Проверка существования сессии
     * @param sessionId идентификатор сессии
     * @return true если сессия существует
     */
    bool hasSession(const SessionId& sessionId) const;
    
    /**
     * Получение списка всех активных сессий
     * @return вектор идентификаторов сессий
     */
    std::vector<SessionId> getActiveSessions() const;
    
    /**
     * Установка коллбэка для вывода
     * @param callback функция для обработки вывода сессий
     */
    void setOutputCallback(OutputCallback callback);
    
    /**
     * Установка коллбэка для изменения статуса
     * @param callback функция для обработки изменения статуса сессий
     */
    void setStatusCallback(StatusCallback callback);
    
    /**
     * Получение статистики
     */
    struct Stats {
        size_t totalSessions;
        size_t activeSessions;
        size_t pollCycles;
        double avgPollTimeMs;
    };
    
    Stats getStats() const;

private:
    /**
     * Основной цикл опроса сессий
     */
    void pollLoop();
    
    /**
     * Опрос одной сессии
     * @param sessionId идентификатор сессии
     * @param session указатель на сессию
     */
    void pollSession(const SessionId& sessionId, std::shared_ptr<TerminalSession> session);
    
    /**
     * Очистка завершенных сессий
     */
    void cleanupFinishedSessions();

private:
    mutable std::mutex sessionsMutex;
    std::unordered_map<SessionId, std::shared_ptr<TerminalSession>> sessions;
    
    std::atomic<bool> running{false};
    std::unique_ptr<std::thread> pollThread;
    int pollIntervalMs;
    
    OutputCallback outputCallback;
    StatusCallback statusCallback;
    
    // Статистика
    mutable std::mutex statsMutex;
    Stats stats{0, 0, 0, 0.0};
    
    // TODO: Добавить в будущем:
    // - std::chrono::steady_clock::time_point m_lastCleanup; // Время последней очистки
    // - size_t m_maxSessions = 100; // Лимит сессий
    // - std::unordered_map<SessionId, std::chrono::steady_clock::time_point> m_sessionActivity; // Активность сессий
    // - bool m_autoCleanup = true; // Автоочистка неактивных сессий
    // - std::chrono::seconds m_sessionTimeout{3600}; // Таймаут сессий
}; 