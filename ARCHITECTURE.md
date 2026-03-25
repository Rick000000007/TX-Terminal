# TX Terminal Architecture

This document describes the architecture and design decisions of TX Terminal.

## Overview

TX Terminal is a high-performance terminal emulator for Android built with:
- **Kotlin + Jetpack Compose** for the UI layer
- **C++** for the core terminal engine
- **JNI** for bridging between Kotlin and C++
- **OpenGL ES 3.0** for hardware-accelerated rendering

## Design Principles

1. **Performance First**: Native C++ core with minimal JNI overhead
2. **Modern Android**: Use latest Android APIs and best practices
3. **User Experience**: Smooth 60fps rendering, responsive UI
4. **Extensibility**: Plugin-ready architecture
5. **Testability**: Comprehensive unit and integration tests

## Layer Architecture

### Layer 1: Presentation (Kotlin)

**Responsibility**: User interface and interaction

**Components**:
- `MainActivity`: Entry point, hosts Compose UI
- `TerminalViewModel`: State management for terminal sessions
- `TerminalSurface`: Custom view for terminal rendering
- `ExtraKeysBar`: Special keys input
- `TabBar`: Session management UI

**Key Technologies**:
- Jetpack Compose for declarative UI
- ViewModel for state management
- DataStore for preferences
- Coroutines for async operations
- Flow for reactive streams

### Layer 2: Domain (Kotlin)

**Responsibility**: Business logic and data models

**Components**:
- `TerminalSession`: Represents a single terminal session
- `SessionManager`: Manages multiple sessions
- `TerminalPreferences`: User settings
- `NativeTerminal`: JNI interface
- `RuntimeBootstrap`: App-private runtime environment setup

**Design Patterns**:
- Repository pattern for data access
- Observer pattern for state updates
- Singleton for managers

### Layer 2.5: Runtime Bootstrap (Kotlin)

**Responsibility**: App-private Linux-style runtime environment

**Components**:
- `RuntimeBootstrap`: Manages runtime directory structure
- `BootstrapState`: Tracks bootstrap progress and state

**Features**:
- Creates HOME, PREFIX, TMP directories
- Sets up environment variables (PATH, HOME, TERM, etc.)
- Extracts bundled tools (BusyBox, etc.)
- Creates applet symlinks for BusyBox
- ABI-aware tool selection (arm64-v8a, armeabi-v7a, x86_64)

**Directory Structure**:
```
/data/data/com.tx.terminal/files/
├── home/          # User's HOME directory
├── usr/           # PREFIX directory
│   ├── bin/       # Executable binaries
│   ├── lib/       # Libraries
│   ├── etc/       # Configuration files
│   └── var/       # Variable data
└── tmp/           # Temporary directory
```

### Layer 3: JNI Bridge (C++/JNI)

**Responsibility**: Bridge between Kotlin and native code

**Components**:
- `tx_jni.cpp`: JNI method implementations
- `TerminalInstance`: Native terminal wrapper
- EGL context management

**Thread Safety**:
- All JNI calls are thread-safe
- Mutex protection for instance map
- Separate reader thread for PTY I/O

### Layer 4: Core Engine (C++)

**Responsibility**: Terminal emulation logic

**Components**:
- `PTY`: Pseudo-terminal management
- `Parser`: ANSI/VT100 escape sequence parser
- `Screen`: Terminal screen buffer
- `Renderer`: OpenGL ES rendering
- `Terminal`: Main orchestrator

**Performance Optimizations**:
- NEON SIMD for ARM64
- Damage tracking for minimal redraws
- Glyph atlas for efficient text rendering
- Instanced rendering for GPU efficiency

## Data Flow

```
User Input (Touch/Keyboard)
    ↓
Jetpack Compose UI
    ↓
ViewModel (State Management)
    ↓
JNI Bridge (tx_jni.cpp)
    ↓
Core Engine (C++)
    ↓
PTY (/dev/ptmx) ←→ Shell Process
```

## Rendering Pipeline

```
1. Screen Update (C++)
   - Parser processes escape sequences
   - Screen buffer updated
   - Damage region calculated

2. JNI Callback
   - onScreenUpdate() triggered
   - Kotlin ViewModel notified

3. Surface Rendering
   - TerminalSurface invalidated
   - Native render() called via JNI

4. OpenGL ES
   - Glyph atlas bound
   - Instanced draw call
   - eglSwapBuffers()
```

## Threading Model

### Android Main Thread
- UI rendering (Compose)
- User input handling
- Lifecycle callbacks

### JNI Reader Thread (per session)
- PTY read operations
- Parser execution
- Screen buffer updates

### Native Render Thread
- OpenGL ES context
- Frame rendering
- Buffer swaps

### Background Threads
- I/O operations
- Network requests (SSH)
- File operations

## Memory Management

### Kotlin Layer
- Lifecycle-aware ViewModels
- Automatic garbage collection
- Flow cancellation on cleanup

### JNI Layer
- Global references for long-lived objects
- Proper cleanup in destructors
- Instance map management

### Native Layer
- RAII for resource management
- Smart pointers (std::unique_ptr)
- Explicit cleanup in TerminalInstance

## Security Considerations

### Process Isolation
- Each session runs in separate PTY
- Shell process is child of app
- No root privileges required

### Input Sanitization
- Escape sequence validation
- Buffer size limits
- Timeout handling

### Memory Safety
- Bounds checking on buffers
- Null pointer checks
- Exception handling in JNI

## Performance Targets

| Metric | Target | Current |
|--------|--------|---------|
| Frame Rate | 60 FPS | 60 FPS |
| Input Latency | < 16ms | ~8ms |
| Memory Usage | < 100MB | ~50MB |
| Startup Time | < 1s | ~500ms |
| Scrollback | 10K lines | 10K lines |

## Extension Points

### Adding New Escape Sequences
1. Add to `Parser::handleCSI()` in `parser.cpp`
2. Implement action in `Terminal::createParserActions()`
3. Add test in `test_parser.cpp`

### Adding New UI Components
1. Create Composable in `ui/components/`
2. Add to `MainScreen.kt`
3. Connect to ViewModel if needed

### Adding New Settings
1. Add key to `TerminalPreferences.kt`
2. Add UI in `SettingsDialog.kt`
3. Apply setting in ViewModel

## Testing Strategy

### Unit Tests (C++)
- Parser state machine
- Screen buffer operations
- PTY mock tests

### Unit Tests (Kotlin)
- ViewModel logic
- Session management
- Preferences handling

### Integration Tests
- JNI bridge
- End-to-end terminal session
- Rendering pipeline

### UI Tests
- Compose UI interactions
- Screen navigation
- Dialog handling

## Build System

### Gradle (Android)
- Multi-module project
- CMake integration for native code
- Product flavors for different architectures

### CMake (Native)
- Separate build for core library
- Android NDK toolchain
- Cross-compilation support

## Debugging

### Android Studio
- Native debugging with LLDB
- Memory profiler
- GPU profiler

### Log Tags
- `TX_JNI`: JNI bridge
- `TX_PTY`: PTY operations
- `TX_Renderer`: Rendering
- `TerminalSession`: Session management
- `TerminalViewModel`: ViewModel state

## Future Architecture

### Plugin System
```
┌─────────────────────────────────┐
│         Plugin Manager          │
├─────────────────────────────────┤
│  ┌─────────┐    ┌─────────┐    │
│  │  SSH    │    │  File   │    │
│  │ Plugin  │    │ Manager │    │
│  └─────────┘    └─────────┘    │
└─────────────────────────────────┘
```

### Multi-Window Support
- Floating terminal windows
- Split-screen sessions
- Picture-in-picture mode

### Cloud Sync
- Settings synchronization
- Session state backup
- Cross-device history

## References

- [ECMA-48](https://www.ecma-international.org/publications-and-standards/standards/ecma-48/): Control Functions
- [VT100 User Guide](https://vt100.net/docs/vt100-ug/): Terminal emulation
- [Android NDK](https://developer.android.com/ndk): Native development
- [Jetpack Compose](https://developer.android.com/jetpack/compose): UI toolkit
