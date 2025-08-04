#include "TerminalSession.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

std::atomic<bool> shouldExit{false};

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        shouldExit = true;
    }
}

// Простейший менеджер сессий для демонстрации
class SimpleSessionManager {
private:
    std::unordered_map<std::string, std::shared_ptr<TerminalSession>> sessions;
    
public:
    bool createSession(const std::string& sessionId, const std::string& command = "bash") {
        if (sessions.find(sessionId) != sessions.end()) {
            return false; // Сессия уже существует
        }
        
        auto session = std::make_shared<TerminalSession>();
        if (session->startCommand(command)) {
            sessions[sessionId] = session;
            std::cout << "Создана сессия: " << sessionId << " (PID: " << session->getChildPid() << ")\n";
            return true;
        }
        return false;
    }
    
    void pollSessions() {
        for (auto& [sessionId, session] : sessions) {
            if (session->hasData(0)) {
                std::string output = session->readOutput();
                if (!output.empty()) {
                    std::cout << "Вывод сессии " << sessionId << ": " << output << "\n";
                }
            }
        }
    }
    
    size_t getActiveSessionCount() const {
        size_t count = 0;
        for (const auto& [sessionId, session] : sessions) {
            if (session->isRunning()) {
                count++;
            }
        }
        return count;
    }
};

int main(int argc, char* argv[])
{
    // Устанавливаем обработчик сигналов
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "=== TermiHUI Server ===\n";
    std::cout << "Сервер готов к работе\n";
    std::cout << "Порт: готов к добавлению WebSocket (пока отключен)\n";
    std::cout << "Для остановки нажмите Ctrl+C\n\n";
    
    // Создаем менеджер сессий
    SimpleSessionManager sessionManager;
    
    std::cout << "Сервер запущен! Ожидание подключений...\n";
    std::cout << "TODO: Добавить WebSocket сервер для подключения клиентов\n\n";
    
    // Основной цикл сервера
    while (!shouldExit) {
        // Опрашиваем активные сессии
        sessionManager.pollSessions();
        
        // Выводим статистику каждые 30 секунд
        static auto lastStatsTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastStatsTime > std::chrono::seconds(30)) {
            size_t activeSessions = sessionManager.getActiveSessionCount();
            std::cout << "Активных сессий: " << activeSessions << "\n";
            lastStatsTime = now;
        }
        
        // Небольшая пауза чтобы не нагружать CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\n=== Завершение сервера ===\n";
    std::cout << "Сервер остановлен\n";
    return 0;
}

// TODO: Будущие улучшения для интеграции с WebSocket-сервером:
// 1. Добавить WebSocket-сервер для удаленного доступа к терминалу
// 2. Реализовать обработку resize-событий от веб-клиента
// 3. Добавить поддержку множественных сессий
// 4. Интегрировать с AI-агентом для анализа команд и вывода
// 5. Добавить аутентификацию и авторизацию
// 6. Реализовать логирование всех операций
// 7. Добавить настройки безопасности (chroot, ограничения команд)
// 8. Поддержка цепочки команд с sudo
// 9. Интеграция с системой мониторинга
// 10. Добавить поддержку сохранения истории команд 