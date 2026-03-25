# Installation Guide

## Linux (Desktop)

### Prerequisites

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential libglfw3-dev \
    libfreetype6-dev libharfbuzz-dev pkg-config
```

#### Fedora
```bash
sudo dnf install cmake gcc-c++ glfw-devel freetype-devel harfbuzz-devel
```

#### Arch Linux
```bash
sudo pacman -S cmake glfw-x11 freetype2 harfbuzz
```

### Build from Source

```bash
git clone https://github.com/yourusername/tx.git
cd tx
./build.sh
```

Or manually:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

### Pre-built Binaries

Download from [Releases](https://github.com/yourusername/tx/releases):

```bash
# x86_64
wget https://github.com/yourusername/tx/releases/download/v1.0.0/tx-linux-x86_64.tar.gz
tar -xzf tx-linux-x86_64.tar.gz
sudo cp tx-linux-x86_64/tx /usr/local/bin/

# aarch64
wget https://github.com/yourusername/tx/releases/download/v1.0.0/tx-linux-aarch64.tar.gz
tar -xzf tx-linux-aarch64.tar.gz
sudo cp tx-linux-aarch64/tx /usr/local/bin/
```

## Android

### From APK

1. Download the APK for your architecture:
   - `tx-android-arm64-v8a.apk` (most modern devices)
   - `tx-android-armeabi-v7a.apk` (older ARM devices)
   - `tx-android-x86_64.apk` (emulators)

2. Install via ADB:
```bash
adb install tx-android-arm64-v8a.apk
```

Or transfer to device and install manually (enable "Unknown sources" in settings).

### From Source

```bash
cd android
./gradlew assembleRelease
```

APK will be at `app/build/outputs/apk/release/app-release-unsigned.apk`

## Post-Installation

### Configuration

Create config file at `~/.config/tx/tx.conf`:

```ini
font.size = 14.0
font.path = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
color.background = 0x000000
color.foreground = 0xFFFFFF
terminal.cols = 80
terminal.rows = 24
```

### Shell Integration

Add to your shell's config file (`.bashrc`, `.zshrc`, etc.):

```bash
# For 256 color support
export TERM=xterm-256color
export COLORTERM=truecolor
```

## Troubleshooting

### Linux

**Issue**: `error while loading shared libraries`

**Solution**: Install missing dependencies:
```bash
# Ubuntu/Debian
sudo apt-get install libglfw3 libfreetype6 libharfbuzz0b

# Fedora
sudo dnf install glfw freetype harfbuzz
```

**Issue**: `Failed to initialize GLFW`

**Solution**: Ensure you're running in a graphical environment or use X forwarding.

### Android

**Issue**: "App not installed"

**Solution**: Uninstall any existing version first, or use `adb install -r` to replace.

**Issue**: Black screen on startup

**Solution**: Check that your device supports OpenGL ES 3.0.

## Uninstallation

### Linux
```bash
sudo rm /usr/local/bin/tx
rm -rf ~/.config/tx
```

### Android
```bash
adb uninstall com.tx.terminal
```
