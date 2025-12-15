#include "TermihuiServer.h"
#include <csignal>
#include <fmt/core.h>
#include "hv/hlog.h"

int main(int argc, char* argv[])
{
    // Disable automatic libhv logs
    hlog_disable();
    
    // Set up signal handlers
    signal(SIGINT, TermihuiServer::signalHandler);
    signal(SIGTERM, TermihuiServer::signalHandler);
    
    fmt::print("=== TermiHUI Server ===\n");
    fmt::print("Press Ctrl+C to stop\n\n");
    
    // Create and start the server
    constexpr int port = 37854;
    TermihuiServer termihuiServer(port);
    
    if (!termihuiServer.start()) {
        return 1;
    }
    
    fmt::print("Server started! Waiting for connections...\n\n");
    
    // Main server loop
    while (!termihuiServer.shouldStop()) {
        termihuiServer.update();
    }
    
    fmt::print("\n=== Server shutdown ===\n");
    termihuiServer.stop();
    
    return 0;
}

// TODO: Future improvements:
// 1. Add support for multiple sessions (tabs)
// 2. Integrate with AI agent for command and output analysis
// 3. Add authentication and authorization
// 4. Implement logging of all operations
// 5. Add security settings (chroot, command restrictions)
// 6. Support for command chains with sudo
// 7. Integration with monitoring system
// 8. Add support for persistent command history
