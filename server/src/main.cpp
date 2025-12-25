#include "TermihuiServerController.h"
#include <csignal>
#include <memory>
#include <fmt/core.h>
#include "hv/hlog.h"
#include <cstring>
#include <string_view>

void printUsage(std::string_view programName) {
    fmt::print("Usage: {} [options]\n", programName);
    fmt::print("Options:\n");
    fmt::print("  -b, --bind <address>   Bind address (default: 127.0.0.1)\n");
    fmt::print("  -p, --port <port>      Port number (default: 37854)\n");
    fmt::print("  -h, --help             Show this help message\n");
    fmt::print("\nExamples:\n");
    fmt::print("  {}                       # Listen on localhost:37854\n", programName);
    fmt::print("  {} -b 0.0.0.0            # Listen on all interfaces\n", programName);
    fmt::print("  {} -b 0.0.0.0 -p 8080    # Listen on all interfaces, port 8080\n", programName);
    }

int main(int argc, char* argv[])
{
    // Default values
    std::string bindAddress = "127.0.0.1";
    int port = 37854;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bind") == 0) {
            if (i + 1 < argc) {
                bindAddress = argv[++i];
            } else {
                fmt::print(stderr, "Error: --bind requires an address argument\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = std::atoi(argv[++i]);
                if (port <= 0 || port > 65535) {
                    fmt::print(stderr, "Error: Invalid port number\n");
                    return 1;
                }
            } else {
                fmt::print(stderr, "Error: --port requires a port number argument\n");
                return 1;
            }
        } else {
            fmt::print(stderr, "Error: Unknown option '{}'\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Disable automatic libhv logs
    hlog_disable();
    
    // Set up signal handlers
    signal(SIGINT, TermihuiServerController::signalHandler);
    signal(SIGTERM, TermihuiServerController::signalHandler);
    
    fmt::print("=== TermiHUI Server ===\n");
    fmt::print("Bind address: {}\n", bindAddress);
    fmt::print("Port: {}\n", port);
    fmt::print("Press Ctrl+C to stop\n\n");
    
    // Create and start the server
    auto webSocketServer = std::make_unique<WebSocketServerImpl>(port, bindAddress);
    TermihuiServerController termihuiServerController(std::move(webSocketServer));
    
    if (!termihuiServerController.start()) {
        return 1;
    }
    
    fmt::print("Server started! Waiting for connections...\n\n");
    
    // Main server loop
    while (!termihuiServerController.shouldStop()) {
        termihuiServerController.update();
                }
    
    fmt::print("\n=== Server shutdown ===\n");
    termihuiServerController.stop();
    
    return 0;
}

// TODO: Future improvements:
// 1. Add support for multiple sessions (tabs)
// 2. Integrate with AI agent for command and output analysis
// 3. Add authentication and authorization (SSL keys like SSH)
// 4. Implement logging of all operations
// 5. Add security settings (chroot, command restrictions)
// 6. Support for command chains with sudo
// 7. Integration with monitoring system
// 8. Add support for persistent command history
