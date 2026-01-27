<h1 align="center">TermiHUI</h1>

<p align="center">
  <strong>A modern, native terminal with GUI — think of it as tmux with a graphical interface</strong>
</p>

<p align="center">
  <a href="#features">Features</a> •
  <a href="#architecture">Architecture</a> •
  <a href="#building">Building</a> •
  <a href="#roadmap">Roadmap</a> •
  <a href="#contributing">Contributing</a> •
  <a href="#license">License</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-macOS%20%7C%20Linux%20%7C%20Windows%20%7C%20iOS%20%7C%20Android-blue" alt="Platforms" />
  <img src="https://img.shields.io/badge/Language-C%2B%2B20%20%7C%20Swift-orange" alt="Languages" />
  <img src="https://img.shields.io/badge/License-MIT-green" alt="License" />
</p>

---

## Why TermiHUI?

I've been thinking about this project for years. Let me share the pain points that led me here.

**tmux is amazing, but it's still TUI.** The moment you need modern UX — drag and drop, visual tab management, smooth animations — you hit a wall. tmux solves session persistence brilliantly, but it's fundamentally limited by being text-based. I wanted something that keeps the power of tmux but isn't trapped in the terminal.

**Modern terminal emulators have a dilemma.** Either they have mediocre UX, or they're built on Electron and similar non-native technologies. I'm going strictly native. Native means familiar UX (macOS users know what I mean), instant response times, and respecting the platform conventions your OS already taught you.

**Tabs belong on the side, not the top.** Top tabs have failed us. On macOS, the top edge of the screen is contested territory — try clicking a tab when Finder's menu bar keeps expanding. And flat tab lists don't scale. I want hierarchical tabs (inspired by [Horse Browser](https://browser.horse/)) that let you actually organize dozens of terminal sessions without losing your mind.

**AI chat for every tab.** But here's the thing — you bring your own keys (BYOK) or run self-hosted models. No vendor lock-in, no surprise bills. You configure which tools the AI can use, and all execution permissions are **closed by default**. You grant them explicitly. Your terminal will never accidentally `rm -rf` anything.

**Mesh network of terminals.** If you're testing distributed systems across multiple hosts, you know the pain of SSH-ing into five machines and running commands in sequence. TermiHUI lets you connect servers together and orchestrate commands across your entire infrastructure from one place.

**The end goal?** Pick up your phone, connect to your server, speak your task, close the app. The task keeps running. When it's done — or when it needs permission for a sub-command — you get a push notification. That's the terminal experience I want to build.

| | Traditional Terminal | tmux | TermiHUI |
|--|---------------------|------|----------|
| Session persistence | ❌ | ✅ | ✅ |
| Visual session management | ❌ | ❌ | ✅ |
| Native performance | ✅ | ✅ | ✅ |
| Cross-platform GUI | ❌ | ❌ | ✅ |
| Mobile + push notifications | ❌ | ❌ | 🚧 |
| AI integration | ❌ | ❌ | 🚧 |

---

## Features

| Feature | Status | Description |
|---------|--------|-------------|
| **Client-Server Architecture** | ✅ | Server manages PTY sessions, clients connect via WebSocket |
| **Native Clients** | ✅ | No Electron — pure native apps (Swift/AppKit for macOS) |
| **Session Management** | ✅ | Create, switch, and manage multiple terminal sessions |
| **Command Blocks** | ✅ | Visual separation of commands and their output |
| **Cross-Block Text Selection** | ✅ | Select and copy text across multiple command blocks |
| **Tab Completion** | ✅ | Smart completion from PATH executables and shell builtins |
| **Command History** | ✅ | Persistent history within server runtime |
| **Terminal Resize** | ✅ | Dynamic terminal size adjustment |
| **Full UTF-8 Support** | ✅ | Including emoji and non-Latin scripts |
| **Hierarchical Tabs** | ⏳ | Tab grouping and organization (sidebar, not top bar) |
| **Unit Tests** | ✅ | Comprehensive test coverage for server and client-core |
| **Interactive Apps** | 🚧 | Support for vim, htop, and other TUI applications (bug reports welcome!) |
| **AI Chat Sidebar** | 🚧 | Chat with AI in each tab, BYOK (Bring Your Own Key) |
| **Mobile Clients** | ⏳ | iOS and Android apps with push notifications |
| **Linux & Windows Clients** | ⏳ | GTK and WinUI native apps |
| **Mesh Network** | ⏳ | Connect servers together for distributed command execution — run scripts across multiple machines in sequence, test distributed systems, orchestrate deployments |
| **AI Skills** | 💡 | Specialized AI capabilities for common workflows |
| **Voice Control** | 💡 | Whisper integration for voice commands |

**Legend:** ✅ Done | 🚧 In Progress | ⏳ Planned | 💡 Idea

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         TermiHUI                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐     WebSocket      ┌──────────────────────┐   │
│  │   Clients    │◄──────────────────►│       Server         │   │
│  │              │                    │                      │   │
│  │  • macOS     │                    │  • Session Manager   │   │
│  │  • Linux     │                    │  • PTY Controller    │   │
│  │  • Windows   │                    │  • ANSI Parser       │   │
│  │  • iOS       │                    │  • Completion Engine │   │
│  │  • Android   │                    │  • History Storage   │   │
│  │              │                    │                      │   │
│  └──────────────┘                    └──────────────────────┘   │
│         │                                      │                │
│         │                                      │                │
│         ▼                                      ▼                │
│  ┌──────────────┐                    ┌──────────────────────┐   │
│  │ Client-Core  │                    │    PTY Process       │   │
│  │   (C++ lib)  │                    │    (bash/zsh)        │   │
│  └──────────────┘                    └──────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Components

| Component | Description | Tech Stack |
|-----------|-------------|------------|
| **Server** | Manages PTY sessions, handles WebSocket connections | C++20, libhv, SQLite |
| **Client-Core** | Shared C++ library for all clients | C++20 |
| **macOS Client** | Native desktop application | Swift, AppKit, SnapKit |
| **Shared** | Protocol definitions, serialization | C++20 |

---

## Building

### Requirements

- CMake 3.16+
- C++20 compatible compiler (Clang 14+, GCC 11+)
- Xcode 14+ (for macOS client)

### Build Server

```bash
# Clone repository
git clone https://github.com/user/termihui.git
cd termihui

# Build server
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run server
./server/termihui-server
```

Server starts on port **37854** (WebSocket).

### Build macOS Client

```bash
# Open Xcode project
open macos-client/termihui-client.xcodeproj

# Build and run (Cmd+R)
```

---

## Usage

### Quick Start

1. **Start the server:**
   ```bash
   ./termihui-server
   ```

2. **Launch the client** and connect to `localhost:37854`

3. **Use the terminal** — commands are visually separated into blocks

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| `Enter` | Execute command |
| `Tab` | Autocomplete |
| `↑` / `↓` | Navigate history |
| `Cmd+C` | Copy selection |
| `Cmd+V` | Paste |

---

## Roadmap

See [FEATURES.md](FEATURES.md) for detailed roadmap.

### Phase 1 — MVP (Current)
- [x] Server + Client-Core
- [x] macOS client (basic functionality)
- [x] Unit test coverage
- [ ] Stabilization, bug fixes

### Phase 2 — AI Integration
- [x] AI chat sidebar
- [ ] Command generation from natural language
- [ ] BYOK providers (OpenAI, Anthropic, local LLMs)
- [ ] AI Skills (specialized agents)

### Phase 3 — Platform Expansion
- [ ] Linux client (GTK)
- [ ] Android client
- [ ] iOS client
- [ ] Push notifications

### Phase 4 — Mesh & Orchestration
- [ ] Server-to-server connections
- [ ] Reverse tunnel (access servers behind NAT)
- [ ] Multi-server command orchestration
- [ ] Distributed script execution

---

## Contributing

Contributions are welcome! Whether it's bug reports, feature requests, or pull requests — all contributions help make TermiHUI better.

### Code Style

- **C++**: Modern C++20 practices
- **Swift**: Apple's Swift API Design Guidelines
- **Commits**: Conventional commits format

### Testing

We aim for comprehensive test coverage. When contributing new features, please include appropriate unit tests.

---

## Project Structure

```
termihui/
├── server/              # C++ server
│   ├── src/             # Server source files
│   └── tests/           # Server tests (Catch2)
├── client-core/         # Shared C++ client library
│   ├── include/         # Public headers
│   ├── src/             # Implementation
│   └── tests/           # Client-core tests
├── shared/              # Protocol definitions
│   └── include/         # Shared headers
├── macos-client/        # macOS native client
│   ├── Shared/          # Cross-platform Swift code
│   └── termihui-client/ # macOS-specific code
└── FEATURES.md          # Detailed roadmap
```

---

## Acknowledgments

- [libhv](https://github.com/ithewei/libhv) — High-performance network library
- [fmt](https://github.com/fmtlib/fmt) — Modern formatting library
- [SQLite](https://sqlite.org/) — Embedded database
- [SnapKit](https://github.com/SnapKit/SnapKit) — Auto Layout DSL for Swift
- [Catch2](https://github.com/catchorg/Catch2) — Testing framework

---

## License

This project is licensed under the MIT License — see the [LICENSE](LICENSE) file for details.

---

<p align="center">
  <sub>Built with ❤️ for developers who love their terminal</sub>
</p>
