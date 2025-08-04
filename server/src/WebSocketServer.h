#pragma once

#include <string>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>

// Forward declarations для libhv
struct websocket_conn_t;

/**
 * WebSocket сервер для терминальных сессий
 * 
 * Особенности:
 * - Обработка WebSocket соединений
 * - JSON протокол для управления сессиями
 * - Многопоточная обработка
 * - Интеграция с SessionManager
 */
class WebSocketServer {
public:
    using MessageCallback = std::function<void(const std::string& clientId, const std::string& message)>;
    using ConnectionCallback = std::function<void(const std::string& clientId, bool connected)>;
    
    /**
     * Конструктор
     * @param port порт для WebSocket сервера
     */
    explicit WebSocketServer(int port = 8080);
    
    /**
     * Деструктор
     */
    ~WebSocketServer();
    
    // Запрещаем копирование
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    
    /**
     * Запуск сервера
     * @return true если сервер успешно запущен
     */
    bool start();
    
    /**
     * Остановка сервера
     */
    void stop();
    
    /**
     * Проверка работы сервера
     * @return true если сервер работает
     */
    bool isRunning() const;
    
    /**
     * Отправка сообщения клиенту
     * @param clientId идентификатор клиента
     * @param message сообщение для отправки
     * @return true если сообщение отправлено
     */
    bool sendMessage(const std::string& clientId, const std::string& message);
    
    /**
     * Широковещательная отправка сообщения
     * @param message сообщение для отправки всем клиентам
     */
    void broadcastMessage(const std::string& message);
    
    /**
     * Установка коллбэка для входящих сообщений
     * @param callback функция для обработки сообщений
     */
    void setMessageCallback(MessageCallback callback);
    
    /**
     * Установка коллбэка для соединений
     * @param callback функция для обработки соединений/разрывов
     */
    void setConnectionCallback(ConnectionCallback callback);
    
    /**
     * Получение количества подключенных клиентов
     * @return количество активных соединений
     */
    size_t getConnectedClients() const;
    
    /**
     * Получение списка подключенных клиентов
     * @return список идентификаторов клиентов
     */
    std::vector<std::string> getClientIds() const;

private:
    /**
     * Генерация уникального ID для клиента
     * @return уникальный идентификатор
     */
    std::string generateClientId();

private:
    int port;
    std::atomic<bool> running{false};
    std::unique_ptr<std::thread> serverThread;
    
    // Коллбэки
    MessageCallback messageCallback;
    ConnectionCallback connectionCallback;
    
    // Управление клиентами
    mutable std::mutex clientsMutex;
    std::unordered_map<std::string, websocket_conn_t*> clients;
    
    // TODO: Добавить в будущем:
    // - SSL/TLS поддержка
    // - Аутентификация клиентов
    // - Ограничение количества соединений
    // - Heartbeat для проверки соединений
    // - Компрессия сообщений
    // - Логирование соединений
}; 