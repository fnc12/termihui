#include "WebSocketServer.h"
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

// Включаем libhv заголовки
#include "hv/WebSocketServer.h"
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
        // Создаем сервер в отдельном потоке
        this->serverThread = std::make_unique<std::thread>([this]() {
            hv::WebSocketServer server(this->port);
            
            // Настраиваем коллбэки
            server.onopen = [this](const hv::WebSocketChannelPtr& channel) {
                std::string clientId = generateClientId();
                
                // Сохраняем соединение
                {
                    std::lock_guard<std::mutex> lock(this->clientsMutex);
                    // Временно используем channel.get() как указатель
                    this->clients[clientId] = reinterpret_cast<websocket_conn_t*>(channel.get());
                }
                
                std::cout << "WebSocket подключение: " << clientId << " (адрес: " << channel->peeraddr << ")\n";
                
                if (this->connectionCallback) {
                    this->connectionCallback(clientId, true);
                }
                
                // Сохраняем clientId в контексте соединения
                channel->setContext(clientId);
            };
            
            server.onmessage = [this](const hv::WebSocketChannelPtr& channel, const std::string& msg) {
                auto clientId = channel->getContext<std::string>();
                std::cout << "Получено сообщение от " << clientId << ": " << msg << "\n";
                
                if (this->messageCallback) {
                    this->messageCallback(clientId, msg);
                }
            };
            
            server.onclose = [this](const hv::WebSocketChannelPtr& channel) {
                auto clientId = channel->getContext<std::string>();
                std::cout << "WebSocket отключение: " << clientId << "\n";
                
                // Удаляем соединение
                {
                    std::lock_guard<std::mutex> lock(this->clientsMutex);
                    this->clients.erase(clientId);
                }
                
                if (this->connectionCallback) {
                    this->connectionCallback(clientId, false);
                }
            };
            
            std::cout << "Запуск WebSocket сервера на порту " << this->port << "\n";
            server.run();
        });
        
        // Даем серверу время на запуск
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        std::cout << "WebSocket сервер запущен на порту " << this->port << "\n";
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Ошибка запуска WebSocket сервера: " << e.what() << "\n";
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
        this->clients.clear();
    }
    
    // Ждем завершения серверного потока
    if (this->serverThread && this->serverThread->joinable()) {
        this->serverThread->join();
    }
    
    std::cout << "WebSocket сервер остановлен\n";
}

bool WebSocketServer::isRunning() const
{
    return this->running.load();
}

bool WebSocketServer::sendMessage(const std::string& clientId, const std::string& message)
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    
    auto it = this->clients.find(clientId);
    if (it == this->clients.end()) {
        return false;
    }
    
    // TODO: Реализовать отправку сообщения через libhv
    // Пока что просто выводим в консоль
    std::cout << "Отправка клиенту " << clientId << ": " << message << "\n";
    
    return true;
}

void WebSocketServer::broadcastMessage(const std::string& message)
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    
    for (const auto& [clientId, conn] : this->clients) {
        // TODO: Реализовать отправку сообщения через libhv
        std::cout << "Broadcast клиенту " << clientId << ": " << message << "\n";
    }
}

void WebSocketServer::setMessageCallback(MessageCallback callback)
{
    this->messageCallback = std::move(callback);
}

void WebSocketServer::setConnectionCallback(ConnectionCallback callback)
{
    this->connectionCallback = std::move(callback);
}

size_t WebSocketServer::getConnectedClients() const
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    return this->clients.size();
}

std::vector<std::string> WebSocketServer::getClientIds() const
{
    std::lock_guard<std::mutex> lock(this->clientsMutex);
    
    std::vector<std::string> result;
    result.reserve(this->clients.size());
    
    for (const auto& [clientId, conn] : this->clients) {
        result.push_back(clientId);
    }
    
    return result;
}

std::string WebSocketServer::generateClientId()
{
    auto now = std::time(nullptr);
    std::stringstream ss;
    ss << "client_" << now << "_" << (rand() % 1000);
    return ss.str();
}

 