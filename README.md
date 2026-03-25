# TX Terminal

<p align="center">
  <img src="assets/icon.png" width="128" height="128" alt="TX Terminal Logo">
</p>

<p align="center">
  <b>A high-performance terminal emulator for Android</b>
</p>

<p align="center">
  <a href="https://github.com/yourusername/tx/actions"><img src="https://github.com/yourusername/tx/workflows/Android%20Build/badge.svg" alt="Build Status"></a>
  <a href="https://github.com/yourusername/tx/releases"><img src="https://img.shields.io/github/v/release/yourusername/tx" alt="Latest Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License"></a>
</p>

---

## Features

- **Modern UI**: Built with Jetpack Compose for a smooth, native Android experience
- **Multiple Sessions**: Tab-based interface for managing multiple terminals
- **Full Terminal Support**: Complete ANSI/VT100 escape sequence support
- **Customizable**: Fonts, colors, and behavior settings
- **Extra Keys Bar**: Quick access to special keys (Ctrl, arrows, etc.)
- **Copy/Paste**: Full clipboard integration
- **Performance**: Native C++ core with OpenGL ES rendering
- **Lightweight**: Minimal resource usage
- **Blinking Cursor**: Configurable cursor with blink animation
- **Runtime Bootstrap**: App-private Linux-style runtime environment
- **ABI-Aware**: Optimized for arm64-v8a with future multi-ABI support

## Screenshots

<p align="center">
  <img src="screenshots/main.png" width="250" alt="Main Screen">
  <img src="screenshots/tabs.png" width="250" alt="Tab Management">
  <img src="screenshots/settings.png" width="250" alt="Settings">
</p>

## Download

### Google Play Store
Coming soon

### GitHub Releases
Download the latest APK from [Releases](https://github.com/yourusername/tx/releases)

### Build from Source
```bash
git clone https://github.com/yourusername/tx.git
cd tx/android
./gradlew assembleRelease
```

## Building

### Prerequisites

- Android Studio Hedgehog (2023.1.1) or newer
- Android SDK 34
- NDK r25 or newer
- CMake 3.22+
- JDK 17

### Build Instructions

1. Clone the repository:
```bash
git clone https://github.com/yourusername/tx.git
cd tx
```

2. Open the `android` folder in Android Studio

3. Sync project with Gradle files

4. Build the project:
```bash
cd android
./gradlew assembleDebug
```

The APK will be located at `app/build/outputs/apk/debug/app-debug.apk`

### Build Variants

| Variant | Command | Description |
|---------|---------|-------------|
| Debug | `./gradlew assembleDebug` | Development build with debugging |
| Release | `./gradlew assembleRelease` | Optimized production build |
| Test | `./gradlew test` | Run unit tests |

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        ANDROID UI (Kotlin)                      │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐  │
│  │  Jetpack    │  │  Terminal   │  │  Settings/Preferences   │  │
│  │  Compose    │  │  ViewModel  │  │  (DataStore)            │  │
│  └────────┬────┘  └──────┬──────┘  └─────────────────────────┘  │
│           │              │                                      │
│  ┌────────┴──────────────┴────────────────────────────────────┐ │
│  │                    JNI Bridge (C++)                         │ │
│  └────────────────────────┬────────────────────────────────────┘ │
└───────────────────────────┼─────────────────────────────────────┘
                            │
┌───────────────────────────┼─────────────────────────────────────┐
│                      CORE ENGINE (C++)                          │
│  ┌─────────────┐  ┌───────┴────┐  ┌─────────────┐  ┌─────────┐ │
│  │   PTY       │  │   Parser   │  │   Screen    │  │Renderer │ │
│  │  (/dev/ptmx)│  │(ANSI/VT100)│  │   Buffer    │  │ (GLES3) │ │
│  └─────────────┘  └────────────┘  └─────────────┘  └─────────┘ │
└─────────────────────────────────────────────────────────────────┘
                            │
┌───────────────────────────┼─────────────────────────────────────┐
│                    RUNTIME BOOTSTRAP (Kotlin)                   │
│  ┌─────────────┐  ┌───────┴────┐  ┌─────────────┐  ┌─────────┐ │
│  │    HOME     │  │   PREFIX   │  │     TMP     │  │BusyBox  │ │
│  │  Directory  │  │  (usr/)    │  │ Directory   │  │Tools    │ │
│  └─────────────┘  └────────────┘  └─────────────┘  └─────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## Project Structure

```
TX/
├── android/                    # Android app module
│   ├── app/
│   │   ├── src/main/
│   │   │   ├── java/com/tx/terminal/
│   │   │   │   ├── ui/         # Jetpack Compose UI
│   │   │   │   ├── data/       # Data models and preferences
│   │   │   │   ├── jni/        # JNI bridge
│   │   │   │   └── viewmodel/  # ViewModels
│   │   │   ├── cpp/            # Native C++ code
│   │   │   └── res/            # Android resources
│   │   └── build.gradle        # App build configuration
│   └── build.gradle            # Project build configuration
│
├── src/                        # Shared C++ core
│   ├── pty.cpp                 # PTY implementation
│   ├── parser.cpp              # ANSI parser
│   ├── screen.cpp              # Screen buffer
│   ├── renderer.cpp            # OpenGL renderer
│   ├── terminal.cpp            # Terminal orchestration
│   └── config.cpp              # Configuration
│
├── include/tx/                 # C++ headers
│   ├── common.hpp
│   ├── pty.hpp
│   ├── parser.hpp
│   ├── screen.hpp
│   ├── renderer.hpp
│   └── terminal.hpp
│
└── tests/                      # Unit tests
    ├── test_parser.cpp
    ├── test_screen.cpp
    └── test_pty.cpp
```

## Runtime Bootstrap

TX Terminal creates an app-private Linux-style runtime environment on first launch:

### Directory Structure

```
/data/data/com.tx.terminal/files/
├── home/          # User's HOME directory (~)
│   ├── .profile   # Shell profile
│   └── .bashrc    # Bash configuration
├── usr/           # PREFIX directory (like /usr)
│   ├── bin/       # Executable binaries
│   ├── lib/       # Libraries
│   ├── etc/       # Configuration files
│   ├── var/       # Variable data
│   └── share/     # Shared data
└── tmp/           # Temporary directory
```

### Environment Variables

The following environment variables are set for each terminal session:

| Variable | Value | Description |
|----------|-------|-------------|
| `HOME` | `/data/data/com.tx.terminal/files/home` | User home directory |
| `PREFIX` | `/data/data/com.tx.terminal/files/usr` | App prefix directory |
| `TMPDIR` | `/data/data/com.tx.terminal/files/tmp` | Temporary directory |
| `PATH` | `$PREFIX/bin:/system/bin` | Executable search path |
| `TERM` | `xterm-256color` | Terminal type |
| `COLORTERM` | `truecolor` | Color support |

### BusyBox Integration

TX Terminal supports BusyBox for a compact multi-tool userspace:

1. Place `busybox` binary in `android/app/src/main/assets/tools/arm64-v8a/`
2. TX will automatically extract and set up applet symlinks
3. Common applets: `ls`, `cat`, `echo`, `ps`, `grep`, `tar`, etc.

## Configuration

TX Terminal can be customized through the Settings UI or by modifying preferences:

### Available Settings

| Setting | Description | Default |
|---------|-------------|---------|
| Font Size | Terminal font size in sp | 14 |
| Background Color | Terminal background color | Black |
| Foreground Color | Terminal text color | White |
| Cursor Color | Cursor color | White |
| Cursor Blink | Enable cursor blinking | false |
| Show Extra Keys | Show extra keys bar | true |
| Vibrate on Keypress | Haptic feedback | true |
| Scrollback Lines | Number of scrollback lines | 10000 |

## Key Bindings

### Soft Keyboard
- Standard Android keyboard input
- Special keys via Extra Keys bar

### Extra Keys Bar
- **ESC**: Escape key
- **TAB**: Tab key
- **Arrows**: Directional navigation
- **HOME/END**: Line navigation
- **PGUP/PGDN**: Page navigation
- **^C**: Send SIGINT (Ctrl+C)
- **^D**: Send EOF (Ctrl+D)
- **^Z**: Suspend process

### Gestures
- **Tap**: Show keyboard
- **Long press**: Context menu
- **Swipe up/down**: Scroll through history

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Make your changes
4. Run tests: `./gradlew test`
5. Commit: `git commit -m 'Add amazing feature'`
6. Push: `git push origin feature/amazing-feature`
7. Open a Pull Request

### Code Style

- **Kotlin**: Follow [Kotlin Coding Conventions](https://kotlinlang.org/docs/coding-conventions.html)
- **C++**: Follow [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- Use `./gradlew ktlintFormat` to format Kotlin code

## Testing

### Unit Tests
```bash
cd android
./gradlew test
```

### Instrumented Tests
```bash
./gradlew connectedAndroidTest
```

### Native Tests
```bash
cd build
make tx_tests
./tests/tx_tests
```

## Roadmap

### Version 1.1
- [ ] SSH client integration
- [ ] Font selection
- [ ] Color schemes
- [ ] URL detection and opening

### Version 1.2
- [ ] Plugin system
- [ ] Custom key mappings
- [ ] Split screen
- [ ] Floating terminal

### Version 2.0
- [ ] X11 forwarding
- [ ] File manager integration
- [ ] Package manager
- [ ] Widget support

## Troubleshooting

### App crashes on startup
- Ensure your device supports OpenGL ES 3.0
- Check that you have sufficient storage space

### Keyboard not appearing
- Tap on the terminal area to focus
- Check system keyboard settings

### Text rendering issues
- Try changing font size in settings
- Ensure your device has sufficient RAM

### Build errors
- Update Android Studio to latest version
- Update NDK to r25 or newer
- Clean and rebuild: `./gradlew clean assembleDebug`

## License

```
MIT License

Copyright (c) 2024 TX Terminal Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Acknowledgments

- [Jetpack Compose](https://developer.android.com/jetpack/compose) - Modern Android UI toolkit
- [Termux](https://termux.dev/) - Inspiration for terminal functionality
- [alacritty](https://github.com/alacritty/alacritty) - Performance inspiration

## Contact

- GitHub Issues: [github.com/yourusername/tx/issues](https://github.com/yourusername/tx/issues)
- Email: tx.terminal@example.com

---

<p align="center">
  Made with ❤️ for the Android community
</p>
