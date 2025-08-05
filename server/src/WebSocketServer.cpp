#include "WebSocketServer.h"
#include <iostream>

// Включаем libhv заголовки
#include "hv/hlog.h"

WebSocketServer::WebSocketServer(int port)
    : port(port)
{
}

WebSocketServer::~WebSocketServer()
{
    stop();
}

bool WebSocketServer::start()
{
    if (this->running.exchange(true)) {
        return false; // Уже запущен
    }
    
    try {
        // Настраиваем callback'и - они будут вызываться в фоновом потоке libhv
        this->wsService.onopen = [this](const WebSocketChannelPtr& channel, const HttpRequestPtr& request) {
            this->onConnection(channel);
        };
        
        this->wsService.onmessage = [this](const WebSocketChannelPtr& channel, const std::string& msg) {
            this->onMessage(channel, msg);
        };
        
        this->wsService.onclose = [this](const WebSocketChannelPtr& channel) {
            this->onClose(channel);
        };
        
        // Настраиваем сервер
        this->wsServer.ws = &this->wsService;
        this->wsServer.port = this->port;
        
        // Запускаем сервер в отдельном потоке
        this->serverThread = std::make_unique<std::thread>([this]() {
            std::cout << "Запуск WebSocket сервера на порту " << this->port << std::endl;
            this->wsServer.run(false); // false = не блокировать поток
        });
        
        std::cout << "WebSocket сервер запущен на порту " << this->port << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка запуска WebSocket сервера: " << e.what() << std::endl;
        this->running = false;
        return false;
    }
}

void WebSocketServer::stop()
{
    if (!this->running.exchange(false)) {
        return; // Уже остановлен
    }
    
    // Закрываем все соединения
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        for (auto& [clientId, channel] : this->clients) {
            try {
                channel->close();
            } catch (const std::exception& e) {
                std::cerr << "Ошибка закрытия соединения " << clientId << ": " << e.what() << std::endl;
            }
        }
        this->clients.clear();
        this->channelToClientId.clear();
    }
    
    // Останавливаем сервер
    this->wsServer.stop();
    
    // Ждем завершения серверного потока
    if (this->serverThread && this->serverThread->joinable()) {
        this->serverThread->join();
    }
    
    // Очищаем все очереди
    {
        std::lock_guard<std::mutex> lock(this->incomingMutex);
        std::queue<IncomingMessage> empty;
        this->incomingQueue.swap(empty);
    }
    
    {
        std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
        std::queue<ConnectionEvent> empty;
        this->connectionEventsQueue.swap(empty);
    }
    
    {
        std::lock_guard<std::mutex> lock(this->outgoingMutex);
        std::queue<OutgoingMessage> empty;
        this->outgoingQueue.swap(empty);
    }
    
    std::cout << "WebSocket сервер остановлен" << std::endl;
}

bool WebSocketServer::isRunning() const
{
    return this->running.load();
}

void WebSocketServer::update(std::vector<IncomingMessage>& incomingMessages,
                           std::vector<ConnectionEvent>& connectionEvents)
{
    // Забираем все входящие сообщения
    {
        std::lock_guard<std::mutex> lock(this->incomingMutex);
        while (!this->incomingQueue.empty()) {
            incomingMessages.push_back(std::move(this->incomingQueue.front()));
            this->incomingQueue.pop();
        }
    }
    
    // Забираем все события подключения
    {
        std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
        while (!this->connectionEventsQueue.empty()) {
            connectionEvents.push_back(std::move(this->connectionEventsQueue.front()));
            this->connectionEventsQueue.pop();
        }
    }
    
    // Обрабатываем исходящие сообщения
    this->processOutgoingMessages();
}

void WebSocketServer::sendMessage(int clientId, const std::string& message)
{
    std::lock_guard<std::mutex> lock(this->outgoingMutex);
    this->outgoingQueue.push({clientId, message});
}

void WebSocketServer::broadcastMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(this->outgoingMutex);
    this->outgoingQueue.push({0, message}); // 0 = broadcast всем
}

size_t WebSocketServer::getConnectedClients() const
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    return this->clients.size();
}

std::vector<int> WebSocketServer::getClientIds() const
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    
    std::vector<int> result;
    result.reserve(this->clients.size());
    
    for (const auto& [clientId, channel] : this->clients) {
        result.push_back(clientId);
    }
    
    return result;
}

int WebSocketServer::generateClientId()
{
    return this->nextClientId.fetch_add(1);
}

void WebSocketServer::onConnection(const WebSocketChannelPtr& channel)
{
    // Генерируем ID для нового клиента    
    int clientId = this->generateClientId();
    
    // Сохраняем соединение
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        this->clients[clientId] = channel;
        this->channelToClientId[channel] = clientId;
    }
    
    std::cout << "WebSocket подключение: " << clientId << " (адрес: " << channel->peeraddr() << ")" << std::endl;
    
    // Добавляем событие в очередь для обработки в главном потоке
    {
        std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
        this->connectionEventsQueue.push({clientId, true});
    }
}

void WebSocketServer::onMessage(const WebSocketChannelPtr& channel, const std::string& message)
{
    // Получаем ID клиента
    int clientId;
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        auto it = this->channelToClientId.find(channel);
        if (it == this->channelToClientId.end()) {
            std::cerr << "Получено сообщение от неизвестного клиента" << std::endl;
            return;
        }
        clientId = it->second;
    }
    
    std::cout << "Получено сообщение от " << clientId << ": " << message << std::endl;
    
    // Добавляем сообщение в очередь для обработки в главном потоке
    {
        std::lock_guard<std::mutex> lock(this->incomingMutex);
        this->incomingQueue.push({clientId, message});
    }
}

void WebSocketServer::onClose(const WebSocketChannelPtr& channel)
{
    // Получаем ID клиента
    int clientId = 0;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(this->clientsMutex);
        auto it = this->channelToClientId.find(channel);
        if (it != this->channelToClientId.end()) {
            clientId = it->second;
            found = true;
            this->channelToClientId.erase(it);
            this->clients.erase(clientId);
        }
    }
    
    if (found) {
        std::cout << "WebSocket отключение: " << clientId << std::endl;
        
        // Добавляем событие в очередь для обработки в главном потоке
        {
            std::lock_guard<std::mutex> lock(this->connectionEventsMutex);
            this->connectionEventsQueue.push({clientId, false});
        }
    }
}

void WebSocketServer::processOutgoingMessages()
{
    std::vector<OutgoingMessage> messages;
    
    // Забираем все исходящие сообщения
    {
        std::lock_guard<std::mutex> lock(this->outgoingMutex);
        while (!this->outgoingQueue.empty()) {
            messages.push_back(std::move(this->outgoingQueue.front()));
            this->outgoingQueue.pop();
        }
    }
    
    // Отправляем сообщения
    std::lock_guard<std::mutex> clientsLock(this->clientsMutex);
    for (const auto& msg : messages) {
        if (msg.clientId == 0) {
            // Broadcast всем клиентам
            for (const auto& [clientId, channel] : this->clients) {
                try {
                    channel->send(msg.message);
                } catch (const std::exception& e) {
                    std::cerr << "Ошибка отправки broadcast сообщения клиенту " << clientId << ": " << e.what() << std::endl;
                }
            }
        } else {
            // Отправляем конкретному клиенту
            auto it = this->clients.find(msg.clientId);
            if (it != this->clients.end()) {
                try {
                    it->second->send(msg.message);
                } catch (const std::exception& e) {
                    std::cerr << "Ошибка отправки сообщения клиенту " << msg.clientId << ": " << e.what() << std::endl;
                }
            } else {
                std::cerr << "Клиент " << msg.clientId << " не найден для отправки сообщения" << std::endl;
            }
        }
    }
}