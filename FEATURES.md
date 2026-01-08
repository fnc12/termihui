# TermiHUI — Roadmap & Features

## Legend

- ✅ Done
- 🚧 In Progress
- ⏳ Planned
- 💡 Idea

---

## Core

### Server
| Feature | Status | Description |
|---------|--------|-------------|
| Basic HTTP/WebSocket server | ✅ Done | libhv, connection handling |
| Session management | ✅ Done | Create, switch, delete sessions |
| PTY (pseudo-terminal) | ✅ Done | Shell interaction |
| Cross-platform (macOS) | ✅ Done | Works on macOS |
| Cross-platform (Linux) | ⏳ Planned | |
| Cross-platform (Windows) | ⏳ Planned | |
| Key-based authentication | ⏳ Planned | Like SSH, WebSocket + keys |
| Configuration file | ⏳ Planned | YAML/TOML config |

### Client-Core (C++ library)
| Feature | Status | Description |
|---------|--------|-------------|
| Server connection | ✅ Done | HTTP/WebSocket client |
| Send commands | ✅ Done | |
| Receive output | ✅ Done | |
| Session management | ✅ Done | |
| Universal binary (arm64 + x86_64) | ✅ Done | For macOS |

---

## Clients

### macOS (Swift/AppKit)
| Feature | Status | Description |
|---------|--------|-------------|
| Basic UI | ✅ Done | Window, toolbar, input field |
| Terminal output display | ✅ Done | NSTextView/ScrollView |
| Session list sidebar | ✅ Done | NSOutlineView |
| Create/switch sessions | ✅ Done | |
| Hamburger menu | ✅ Done | Sidebar toggle button |
| Lazy sidebar loading | ✅ Done | Not created until first click |
| Text selection & copy | 🚧 In Progress | Select and copy output |
| Hierarchical tabs | ⏳ Planned | Tab grouping, not on top |
| Multiple connections | ⏳ Planned | Several servers at once |
| Preferences | ⏳ Planned | Fonts, colors, hotkeys |
| Themes | ⏳ Planned | Light/dark, custom |
| Local notifications | ⏳ Planned | When command completes |

### Linux (GTK/gtkmm)
| Feature | Status | Description |
|---------|--------|-------------|
| Basic UI | ⏳ Planned | |
| Terminal output | ⏳ Planned | |
| Session management | ⏳ Planned | |
| Hierarchical tabs | ⏳ Planned | |

### Windows (WinUI/WinForms)
| Feature | Status | Description |
|---------|--------|-------------|
| Basic UI | ⏳ Planned | |
| Terminal output | ⏳ Planned | |
| Session management | ⏳ Planned | |
| Hierarchical tabs | ⏳ Planned | |

### Android (Kotlin/Jetpack Compose)
| Feature | Status | Description |
|---------|--------|-------------|
| Basic UI | ⏳ Planned | |
| Server connection | ⏳ Planned | |
| View output | ⏳ Planned | |
| Send commands | ⏳ Planned | |
| Push notifications | ⏳ Planned | FCM |

### iOS (Swift/SwiftUI)
| Feature | Status | Description |
|---------|--------|-------------|
| Basic UI | 💡 Idea | Maybe in the future |
| Push notifications | 💡 Idea | APNS |

---

## Mesh Network & Orchestration

| Feature | Status | Description |
|---------|--------|-------------|
| Server-to-server connection | ⏳ Planned | WebSocket, key authentication |
| Reverse tunnel | ⏳ Planned | Like reverse SSH for servers behind NAT |
| Connected servers list | ⏳ Planned | UI to view the network |
| Execute commands on remote server | ⏳ Planned | Through mesh |
| Orchestration (multi-server commands) | ⏳ Planned | One agent controls multiple servers |
| Service sharing between servers | ⏳ Planned | LLM, embedding, whisper |
| ACL for sharing | ⏳ Planned | What to share with whom |

---

## AI Agent

| Feature | Status | Description |
|---------|--------|-------------|
| AI chat in each tab | ⏳ Planned | Integrated chat |
| LLM provider connection | ⏳ Planned | OpenAI-compatible, Gemma, ChatML |
| BYOK (Bring Your Own Key) | ⏳ Planned | Your own API key |
| Local LLM (Ollama) | ⏳ Planned | Offline capable |
| Agent with tools | ⏳ Planned | Execute commands, search, todo list |
| Generate commands from text | ⏳ Planned | "Show files larger than 100mb" → find ... |
| Error explanation | ⏳ Planned | Click on error → AI explains |
| Command autocomplete | ⏳ Planned | AI-powered suggestions |
| Custom agents | ⏳ Planned | Create your own agents |
| Import/export agents | ⏳ Planned | Share configurations |
| Different agents per tab | ⏳ Planned | DevOps agent, Data Science agent |

---

## RAG & Memory

| Feature | Status | Description |
|---------|--------|-------------|
| Vector DB connection | ⏳ Planned | Optional on server |
| Embedding API | ⏳ Planned | For creating embeddings |
| Chat history storage | ⏳ Planned | With embeddings |
| Semantic history search | ⏳ Planned | "How did I deploy yesterday?" |
| Context from past chats | ⏳ Planned | AI remembers previous conversations |

---

## Voice Control

| Feature | Status | Description |
|---------|--------|-------------|
| Whisper API integration | ⏳ Planned | Local or cloud |
| Voice command input | ⏳ Planned | Speak → text → AI |
| Push-to-talk | ⏳ Planned | Hotkey for recording |
| Whisper sharing between servers | ⏳ Planned | One GPU server for all |

---

## Push Notifications

| Feature | Status | Description |
|---------|--------|-------------|
| Push server infrastructure | ⏳ Planned | APNS/FCM gateway |
| Device registration | ⏳ Planned | Link mobile to server |
| Push on command completion | ⏳ Planned | "Notify when done" button |
| Filter settings | ⏳ Planned | Errors only, long commands only |

---

## Marketplace

| Feature | Status | Description |
|---------|--------|-------------|
| Agent catalog | 💡 Idea | Public list |
| Install agents from catalog | 💡 Idea | One-click install |
| Publish your agents | 💡 Idea | |
| Paid agents | 💡 Idea | With commission |
| Ratings and reviews | 💡 Idea | |

---

## UX/UI

| Feature | Status | Description |
|---------|--------|-------------|
| Hierarchical tabs | ⏳ Planned | Grouping, drag & drop |
| Side tab panel | ⏳ Planned | Not on top (macOS menubar issue) |
| Split view | 💡 Idea | Multiple terminals in one window |
| Output search | ⏳ Planned | Cmd+F |
| Command blocks | 🚧 In Progress | Visual command separation |
| Collapse output | 💡 Idea | Hide large output |

---

## Security

| Feature | Status | Description |
|---------|--------|-------------|
| Key-based authentication | ⏳ Planned | ED25519/RSA |
| Traffic encryption | ⏳ Planned | TLS/WSS |
| ACL for mesh servers | ⏳ Planned | Who can connect to whom |
| Audit log | 💡 Idea | Who executed what |

---

## Development Phases

### Phase 1 — MVP (current)
1. ✅ Server + client-core
2. 🚧 macOS client (basic functionality)
3. ⏳ Stabilization, bug fixes

### Phase 2 — Platform Expansion
4. ⏳ Linux client (GTK)
5. ⏳ Android client
6. ⏳ Push notifications (first monetization)

### Phase 3 — AI
7. ⏳ AI chat in tabs
8. ⏳ Basic agent with tools
9. ⏳ BYOK providers

### Phase 4 — Mesh & Orchestration
10. ⏳ Server-to-server connections
11. ⏳ Reverse tunnel
12. ⏳ Multi-server orchestration

### Phase 5 — Advanced AI
13. ⏳ RAG + vector DB
14. ⏳ Whisper integration
15. ⏳ Service sharing

### Phase 6 — Ecosystem
16. 💡 Windows client
17. 💡 iOS client
18. 💡 Agent marketplace

---

## Technical Notes

- **Electron-free** — all clients are native
- **Performance** — no lag on large output (unlike Warp)
- **BYOK** — no token reselling, users connect their own APIs
- **Open source** — core is free, monetization on infrastructure and premium features
