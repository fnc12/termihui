#include "TerminalSession.h"
#include "WebSocketServer.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <sstream>
#include "hv/json.hpp"

std::atomic<bool> shouldExit{false};

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        shouldExit = true;
    }
}

using json = nlohmann::json;

// Вспомогательные функции для работы с JSON
class JsonHelper {
public:
    static std::string createResponse(const std::string& type, const std::string& data = "", int exitCode = 0, bool running = false) {
        json response;
        response["type"] = type;
        
        if (type == "output" && !data.empty()) {
            response["data"] = data;
        } else if (type == "status") {
            response["running"] = running;
            response["exit_code"] = exitCode;
        } else if (type == "error" && !data.empty()) {
            response["message"] = data;
            response["error_code"] = "COMMAND_FAILED";
        } else if (type == "connected") {
            response["server_version"] = "1.0.0";
        } else if (type == "input_sent") {
            response["bytes"] = exitCode;
        }
        
        return response.dump();
    }
};

int main(int argc, char* argv[])
{
    // Устанавливаем обработчик сигналов
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "=== TermiHUI Server ===\n";
    std::cout << "Сервер готов к работе\n";
    std::cout << "Порт: 37854 (WebSocket)\n";
    std::cout << "Для остановки нажмите Ctrl+C\n\n";
    
    // Создаем единственную терминальную сессию
    auto terminalSession = std::make_unique<TerminalSession>();
    
    // Создаем и запускаем WebSocket сервер
    WebSocketServer wsServer(37854);
    if (!wsServer.start()) {
        std::cerr << "Не удалось запустить WebSocket сервер\n";
        return 1;
    }
    
    std::cout << "Сервер запущен! Ожидание подключений...\n\n";
    
    // Основной цикл сервера
    while (!shouldExit) {
        // Получаем события от WebSocket сервера
        std::vector<WebSocketServer::IncomingMessage> incomingMessages;
        std::vector<WebSocketServer::ConnectionEvent> connectionEvents;
        
        wsServer.update(incomingMessages, connectionEvents);
        
        // Обрабатываем события подключения/отключения
        for (const auto& event : connectionEvents) {
            if (event.connected) {
                std::cout << "Клиент подключился: " << event.clientId << std::endl;
                // Отправляем приветственное сообщение
                std::string welcome = JsonHelper::createResponse("connected");
                wsServer.sendMessage(event.clientId, welcome);
            } else {
                std::cout << "Клиент отключился: " << event.clientId << std::endl;
            }
        }
        
        // Обрабатываем входящие сообщения
        for (const auto& msg : incomingMessages) {
            std::cout << "Обработка сообщения от " << msg.clientId << ": " << msg.message << std::endl;
            
            try {
                json msgJson = json::parse(msg.message);
                std::string type = msgJson.value("type", "");
                
                if (type == "execute") {
                    std::string command = msgJson.value("command", "");
                    if (!command.empty()) {
                        if (terminalSession->startCommand(command)) {
                            std::cout << "Запущена команда: " << command << " (PID: " << terminalSession->getChildPid() << ")" << std::endl;
                        } else {
                            std::string error = JsonHelper::createResponse("error", "Failed to start command");
                            wsServer.sendMessage(msg.clientId, error);
                        }
                    } else {
                        std::string error = JsonHelper::createResponse("error", "Missing command field");
                        wsServer.sendMessage(msg.clientId, error);
                    }
                } else if (type == "input") {
                    std::string text = msgJson.value("text", "");
                    if (!text.empty()) {
                        ssize_t bytes = terminalSession->sendInput(text);
                        if (bytes >= 0) {
                            std::string response = JsonHelper::createResponse("input_sent", "", bytes);
                            wsServer.sendMessage(msg.clientId, response);
                        } else {
                            std::string error = JsonHelper::createResponse("error", "Failed to send input");
                            wsServer.sendMessage(msg.clientId, error);
                        }
                    } else {
                        std::string error = JsonHelper::createResponse("error", "Missing text field");
                        wsServer.sendMessage(msg.clientId, error);
                    }
                } else {
                    std::string error = JsonHelper::createResponse("error", "Unknown message type");
                    wsServer.sendMessage(msg.clientId, error);
                }
            } catch (const json::exception& e) {
                std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
                std::string error = JsonHelper::createResponse("error", "Invalid JSON format");
                wsServer.sendMessage(msg.clientId, error);
            }
        }
        
        // Проверяем вывод терминальной сессии
        if (terminalSession->hasData(0)) {
            std::string output = terminalSession->readOutput();
            if (!output.empty()) {
                std::cout << "Вывод терминала: " << output << std::endl;
                std::string response = JsonHelper::createResponse("output", output);
                wsServer.broadcastMessage(response);
            }
        }
        
        // Проверяем статус сессии и отправляем уведомление о завершении
        static bool wasRunning = false;
        bool currentlyRunning = terminalSession->isRunning();
        if (wasRunning && !currentlyRunning) {
            std::cout << "Команда завершена" << std::endl;
            std::string status = JsonHelper::createResponse("status", "", 0, false);
            wsServer.broadcastMessage(status);
        }
        wasRunning = currentlyRunning;
        
        // Выводим статистику каждые 30 секунд
        static auto lastStatsTime = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastStatsTime > std::chrono::seconds(30)) {
            size_t connectedClients = wsServer.getConnectedClients();
            std::cout << "Подключенных клиентов: " << connectedClients << std::endl;
            std::cout << "Терминальная сессия активна: " << (terminalSession->isRunning() ? "да" : "нет") << std::endl;
            lastStatsTime = now;
        }
        
        // Небольшая пауза чтобы не нагружать CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Уменьшено до 10ms для лучшей отзывчивости
    }
    
    std::cout << "\n=== Завершение сервера ===\n";
    wsServer.stop();
    std::cout << "Сервер остановлен\n";
    return 0;
}

// TODO: Будущие улучшения:
// 1. Реализовать обработку resize-событий от клиента
// 2. Добавить поддержку множественных сессий (вкладки)
// 3. Интегрировать с AI-агентом для анализа команд и вывода
// 4. Добавить аутентификацию и авторизацию
// 5. Реализовать логирование всех операций
// 6. Добавить настройки безопасности (chroot, ограничения команд)
// 7. Поддержка цепочки команд с sudo
// 8. Интеграция с системой мониторинга
// 9. Добавить поддержку сохранения истории команд 