#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <queue>
#include <vector>

// Включаем libhv заголовки
#include "hv/WebSocketServer.h"

/**
 * WebSocket сервер для терминальных сессий
 * 
 * Особенности:
 * - Обработка WebSocket соединений через libhv
 * - JSON протокол согласно docs/protocol.md
 * - Неблокирующая архитектура с очередями сообщений
 * - Обработка в главном потоке через update()
 */
class WebSocketServer {
public:
    /**
     * Структура входящего сообщения от клиента
     */
    struct IncomingMessage {
        int clientId;
        std::string message;
    };
    
    /**
     * Структура исходящего сообщения для клиента
     */
    struct OutgoingMessage {
        int clientId;  // 0 = broadcast всем
        std::string message;
    };
    
    /**
     * Событие подключения/отключения клиента
     */
    struct ConnectionEvent {
        int clientId;
        bool connected;  // true = подключился, false = отключился
    };
    
    /**
     * Конструктор
     * @param port порт для WebSocket сервера
     */
    explicit WebSocketServer(int port = 37854);
    
    /**
     * Деструктор
     */
    ~WebSocketServer();
    
    // Запрещаем копирование
    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;
    
    /**
     * Запуск сервера (в фоновом потоке)
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
     * Обновление - обработка всех накопленных событий (вызывать из главного потока)
     * @param incomingMessages [out] новые сообщения от клиентов 
     * @param connectionEvents [out] события подключения/отключения
     */
    void update(std::vector<IncomingMessage>& incomingMessages,
                std::vector<ConnectionEvent>& connectionEvents);
    
    /**
     * Отправка сообщения клиенту (добавляет в очередь)
     * @param clientId идентификатор клиента
     * @param message сообщение для отправки
     */
    void sendMessage(int clientId, const std::string& message);
    
    /**
     * Широковещательная отправка сообщения (добавляет в очередь)
     * @param message сообщение для отправки всем клиентам
     */
    void broadcastMessage(const std::string& message);
    
    /**
     * Получение количества подключенных клиентов
     * @return количество активных соединений
     */
    size_t getConnectedClients() const;
    
    /**
     * Получение списка подключенных клиентов
     * @return список идентификаторов клиентов
     */
    std::vector<int> getClientIds() const;

private:
    /**
     * Генерация уникального ID для клиента
     * @return уникальный идентификатор
     */
    int generateClientId();
    
    /**
     * Callback'и libhv (выполняются в фоновом потоке)
     */
    void onConnection(const WebSocketChannelPtr& channel);
    void onMessage(const WebSocketChannelPtr& channel, const std::string& message);
    void onClose(const WebSocketChannelPtr& channel);
    
    /**
     * Отправка исходящих сообщений из очереди (вызывается из update)
     */
    void processOutgoingMessages();

private:
    int port;
    std::atomic<bool> running{false};
    std::atomic<int> nextClientId{1};
    
    // libhv компоненты
    hv::WebSocketService wsService;
    hv::WebSocketServer wsServer;
    std::unique_ptr<std::thread> serverThread;
    
    // Управление клиентами (защищено мютексом)
    mutable std::mutex clientsMutex;
    std::unordered_map<int, WebSocketChannelPtr> clients;
    std::unordered_map<WebSocketChannelPtr, int> channelToClientId;
    
    // Очереди сообщений (защищены мютексами)
    std::mutex incomingMutex;
    std::queue<IncomingMessage> incomingQueue;
    
    std::mutex connectionEventsMutex;
    std::queue<ConnectionEvent> connectionEventsQueue;
    
    std::mutex outgoingMutex;
    std::queue<OutgoingMessage> outgoingQueue;
    
    // TODO: Добавить в будущем:
    // - SSL/TLS поддержка
    // - Аутентификация клиентов
    // - Ограничение количества соединений
    // - Heartbeat для проверки соединений
    // - Компрессия сообщений
    // - Логирование соединений
}; 