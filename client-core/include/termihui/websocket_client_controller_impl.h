#pragma once

#include "websocket_client_controller.h"
#include "thread_safe_queue.h"
#include <hv/WebSocketClient.h>
#include <memory>

namespace termihui {

/**
 * Implementation of WebSocketClientController using libhv.
 */
class WebSocketClientControllerImpl final : public WebSocketClientController {
public:
    WebSocketClientControllerImpl();
    ~WebSocketClientControllerImpl() override;
    
    int open(std::string_view url) override;
    void close() override;
    bool isConnected() const override;
    int send(const std::string& message) override;
    std::vector<Event> update() override;

private:
    std::unique_ptr<hv::WebSocketClient> client;
    ThreadSafeQueue<Event> eventQueue;
};

} // namespace termihui
