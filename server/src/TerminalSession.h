#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <sys/types.h>
#include "CompletionManager.h"

/**
 * Класс для управления терминальными сессиями через PTY
 * 
 * Особенности:
 * - Асинхронное выполнение команд через forkpty
 * - Неблокирующее чтение вывода
 * - Буферизация данных
 * - Проверка состояния процесса
 */
class TerminalSession {
public:
    /**
     * Конструктор
     * @param bufferSize размер буфера для вывода (по умолчанию 4096)
     */
    explicit TerminalSession(size_t bufferSize = 4096);
    
    /**
     * Деструктор - автоматически завершает сессию
     */
    ~TerminalSession();
    
    // Запрещаем копирование
    TerminalSession(const TerminalSession&) = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;
    
    // Разрешаем перемещение
    TerminalSession(TerminalSession&& other) noexcept;
    TerminalSession& operator=(TerminalSession&& other) noexcept;
    
    /**
     * Создание интерактивной bash-сессии
     * @return true если сессия успешно создана
     */
    bool createSession();
    
    /**
     * Выполнение команды в существующей сессии
     * @param command команда для выполнения
     * @return true если команда успешно отправлена
     */
    bool executeCommand(const std::string& command);
    
    /**
     * Отправка текста в PTY
     * @param input текст для отправки
     * @return количество отправленных байт, -1 при ошибке
     */
    ssize_t sendInput(std::string_view input);
    
    /**
     * Чтение доступного вывода из PTY
     * @return строка с новым выводом
     */
    std::string readOutput();
    
    /**
     * Проверка, запущен ли процесс
     * @return true если процесс активен
     */
    bool isRunning() const;
    
    /**
     * Получение PID дочернего процесса
     * @return PID процесса или -1 если не запущен
     */
    pid_t getChildPid() const;
    
    /**
     * Принудительное завершение сессии
     */
    void terminate();
    
    /**
     * Получение файлового дескриптора PTY (для внешнего polling)
     * @return файловый дескриптор или -1
     */
    int getPtyFd() const;
    
    /**
     * Проверка доступности данных для чтения
     * @param timeoutMs таймаут в миллисекундах (0 = неблокирующий)
     * @return true если данные доступны
     */
    bool hasData(int timeoutMs = 0) const;
    
    /**
     * Получение вариантов автодополнения для текста
     * @param text текст для автодополнения
     * @param cursorPosition позиция курсора в тексте
     * @return вектор строк с вариантами автодополнения
     */
    std::vector<std::string> getCompletions(const std::string& text, int cursorPosition) const;
    
    /**
     * Получение текущей рабочей директории bash-процесса
     * @return путь к текущей директории или пустая строка при ошибке
     */
    std::string getCurrentWorkingDirectory() const;
    
    /**
     * Установка последнего известного cwd (из OSC маркеров)
     * @param cwd путь к директории
     */
    void setLastKnownCwd(const std::string& cwd);
    
    /**
     * Получение последнего известного cwd
     * @return путь или пустая строка
     */
    std::string getLastKnownCwd() const;

private:
    /**
     * Настройка PTY в неблокирующий режим
     */
    void setupNonBlocking();
    
    /**
     * Очистка ресурсов
     */
    void cleanup();
    
    /**
     * Проверка статуса дочернего процесса
     */
    void checkChildStatus();

private:
    int ptyFd;                    // Файловый дескриптор PTY
    pid_t childPid;               // PID дочернего процесса
    std::vector<char> buffer;     // Буфер для чтения данных
    size_t bufferSize;            // Размер буфера
    bool running;                 // Флаг активности процесса
    bool sessionCreated;          // Флаг созданной сессии
    
    // Последний известный cwd из OSC маркеров
    mutable std::string lastKnownCwd;
    
    // Менеджер автодополнения
    CompletionManager completionManager;
    
    // TODO: Добавить в будущем:
    // - struct winsize m_windowSize;  // Размер окна терминала для resize-событий
    // - std::string m_lastCommand;    // Последняя выполненная команда
    // - std::queue<std::string> m_commandQueue; // Очередь команд для цепочки
    // - bool m_sudoMode;              // Режим sudo для привилегированных операций
    // - std::function<void(const std::string&)> m_aiAnalyzer; // Коллбэк для AI-анализа
    // - std::chrono::steady_clock::time_point m_lastActivity; // Время последней активности
    // - std::string m_promptPattern;  // Паттерн для определения окончания команды
}; 