# TermiHUI Server

Server component of TermiHUI project for managing terminal sessions via PTY.

## Features

- **Asynchronous command execution** via `forkpty`
- **Non-blocking output reading** using `fcntl` + `poll`
- **Data buffering** for efficient operation
- **Real-time process state checking**
- **Cross-platform** (macOS/Linux/Windows)
- **WebSocket communication** for client connections
- **Command history** persisted during server runtime
- **Tab completion** from PATH executables and shell builtins
- **Terminal resize** support

## Project Structure

```
server/
├── CMakeLists.txt              # Build configuration
├── src/
│   ├── TerminalSessionController.h       # Terminal session class header
│   ├── TerminalSessionController.cpp     # Terminal session implementation
│   ├── WebSocketServer.h       # WebSocket server header
│   ├── WebSocketServer.cpp     # WebSocket server implementation
│   ├── CompletionManager.h     # Autocompletion manager header
│   ├── CompletionManager.cpp   # Autocompletion implementation
│   ├── JsonHelper.h            # JSON helper utilities header
│   ├── JsonHelper.cpp          # JSON helper implementation
│   └── main.cpp                # Main server entry point
└── README.md                   # This file
```

## Build

### Requirements

- CMake 3.15+
- C++17 compatible compiler (GCC, Clang, MSVC)
- libutil (for forkpty on Unix)

### Build Commands

```bash
cd server
mkdir build
cd build
cmake ..
make
```

### Running

```bash
./termihui-server
```

The server will start on port **37854** (WebSocket).

## Usage Example

### TerminalSessionController Class

```cpp
#include "TerminalSessionController.h"

// Create session
TerminalSessionController session;

// Create interactive bash session
if (session.createSession()) {
    // Execute command
    session.executeCommand("ls -la");
    
    // Read output
    while (session.isRunning()) {
        if (session.hasData()) {
            std::string output = session.readOutput();
            std::cout << output;
        }
    }
}

// Send input (for interactive commands)
session.sendInput("echo 'Hello World!'\n");

// Terminate session
session.terminate();
```

## API Reference

### TerminalSessionController Main Methods

- `createSession()` - Create interactive bash/shell session
- `executeCommand(command)` - Execute command in session
- `sendInput(input)` - Send text to PTY
- `readOutput()` - Read available output
- `isRunning()` - Check if process is active
- `terminate()` - Force terminate session
- `setWindowSize(cols, rows)` - Set terminal dimensions

### Helper Methods

- `getChildPid()` - Get child process PID
- `getPtyFd()` - Get PTY file descriptor
- `hasData()` - Check data availability
- `getCompletions(text, pos)` - Get tab completion options
- `getCurrentWorkingDirectory()` - Get bash process cwd

## WebSocket Protocol

### Client → Server Messages

```json
{"type": "execute", "command": "ls -la"}
{"type": "input", "text": "\t"}
{"type": "completion", "text": "ot", "cursor_position": 2}
{"type": "resize", "cols": 120, "rows": 40}
```

### Server → Client Messages

```json
{"type": "connected", "server_version": "1.0.0", "cwd": "/home/user"}
{"type": "output", "data": "file1.txt\nfile2.txt\n"}
{"type": "command_start", "cwd": "/home/user"}
{"type": "command_end", "exit_code": 0, "cwd": "/home/user"}
{"type": "completion_result", "completions": ["otool", "otctl"]}
{"type": "history", "commands": [...]}
```

## Planned Improvements (TODO)

### Terminal Features
- [ ] Better window resize handling
- [ ] Command prompt pattern detection
- [ ] Sudo command chains support
- [ ] Persistent command history (database)

### Integration
- [ ] Multiple concurrent sessions (tabs)
- [ ] AI integration for command analysis
- [ ] Monitoring and logging system
- [ ] Session management UI

### Security
- [ ] Authentication and authorization
- [ ] Command restrictions (whitelist/blacklist)
- [ ] Chroot environment
- [ ] Resource controls

## Architecture

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Client App    │───▶│ TerminalSessionController  │───▶│   PTY Process   │
│   (WebSocket)   │    │     (C++)        │    │   (bash/zsh)    │
└─────────────────┘    └──────────────────┘    └─────────────────┘
         │                      │
         │                      ▼
         │             ┌──────────────────┐
         │             │ CompletionManager│
         │             │   (PATH cache)   │
         │             └──────────────────┘
         ▼
┌─────────────────┐
│ WebSocketServer │
│   (libhv)       │
└─────────────────┘
```

## License

This project is being developed as open source software.
