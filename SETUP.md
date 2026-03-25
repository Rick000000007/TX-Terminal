# TX Terminal Setup Guide

This guide will help you set up the TX Terminal development environment.

## Prerequisites

### System Requirements

- **OS**: Linux, macOS, or Windows with WSL2
- **RAM**: 8GB minimum, 16GB recommended
- **Storage**: 10GB free space
- **Internet**: Stable connection for downloads

### Required Software

1. **Android Studio** (Hedgehog 2023.1.1 or newer)
   - Download from [developer.android.com/studio](https://developer.android.com/studio)

2. **JDK 17**
   - Can be installed via Android Studio or separately

3. **Git**
   - `sudo apt-get install git` (Ubuntu/Debian)
   - `brew install git` (macOS)

4. **CMake 3.22+**
   - Included with Android Studio NDK

## Installation Steps

### 1. Install Android Studio

```bash
# Ubuntu/Debian
sudo snap install android-studio --classic

# macOS
brew install --cask android-studio

# Or download from official website
```

### 2. Configure Android SDK

Open Android Studio and:
1. Go to `Tools` → `SDK Manager`
2. Install the following:
   - **SDK Platforms**: Android 14.0 (API 34)
   - **SDK Tools**:
     - Android SDK Build-Tools 34
     - NDK (Side by side) 25.2.9519653
     - CMake 3.22.1
     - Android SDK Platform-Tools

### 3. Set Environment Variables

Add to your `~/.bashrc` or `~/.zshrc`:

```bash
export ANDROID_HOME=$HOME/Android/Sdk
export ANDROID_SDK_ROOT=$ANDROID_HOME
export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools

# For NDK
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/25.2.9519653
```

Reload shell:
```bash
source ~/.bashrc  # or ~/.zshrc
```

### 4. Clone Repository

```bash
git clone https://github.com/yourusername/tx.git
cd tx
```

### 5. Build Native Dependencies

```bash
# Initialize submodules if any
git submodule update --init --recursive

# Build C++ core (optional, Android Studio will do this)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
cd ..
```

### 6. Open in Android Studio

1. Launch Android Studio
2. Select `Open an existing Android Studio project`
3. Navigate to `tx/android` folder
4. Click `OK`

### 7. Sync Project

Android Studio will automatically sync the project. If not:
- Click `File` → `Sync Project with Gradle Files`
- Or click the elephant icon in the toolbar

### 8. Build the Project

```bash
# Via command line
cd android
./gradlew assembleDebug

# Or in Android Studio
# Build → Make Project (Ctrl+F9)
```

### 9. Run on Device/Emulator

#### Physical Device
1. Enable Developer Options on your Android device
2. Enable USB Debugging
3. Connect device via USB
4. Click `Run` → `Run 'app'` (Shift+F10)

#### Emulator
1. Open AVD Manager (Tools → Device Manager)
2. Create Virtual Device
3. Select system image (API 26+, ARM64 recommended)
4. Start emulator
5. Click `Run` → `Run 'app'`

## Verification

### Check Build
```bash
cd android
./gradlew assembleDebug

# Should create:
# app/build/outputs/apk/debug/app-debug.apk
```

### Run Tests
```bash
# Unit tests
./gradlew test

# Connected tests (requires device/emulator)
./gradlew connectedAndroidTest
```

### Check Native Build
```bash
# Verify .so files are created
ls app/build/intermediates/cmake/debug/obj/arm64-v8a/
# Should see: libtx_jni.so
```

## Development Workflow

### Making Changes

1. **Kotlin Code**: Edit in `android/app/src/main/java/`
2. **C++ Code**: Edit in `android/app/src/main/cpp/` or `src/`
3. **Resources**: Edit in `android/app/src/main/res/`

### Building Changes

```bash
# Incremental build
./gradlew assembleDebug

# Clean build
./gradlew clean assembleDebug

# Build specific ABI
./gradlew assembleDebug -Pandroid.injected.build.abi=arm64-v8a
```

### Debugging

#### Kotlin Debugging
1. Set breakpoints in Kotlin code
2. Click `Debug` → `Debug 'app'` (Shift+F9)

#### Native Debugging
1. Set breakpoints in C++ code
2. Select `Debug 'app'` with native debugging enabled
3. LLDB will attach to the process

### Logging

View logs in Android Studio:
```
View → Tool Windows → Logcat
```

Filter by tag:
```
tag:TX_JNI
```

## Troubleshooting

### Build Failures

#### "CMake not found"
```bash
# Install via SDK Manager
sdkmanager "cmake;3.22.1"
```

#### "NDK not found"
```bash
# Set NDK path
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/25.2.9519653
```

#### "Gradle sync failed"
```bash
# Clear Gradle cache
./gradlew clean
rm -rf ~/.gradle/caches/
./gradlew assembleDebug
```

### Runtime Issues

#### "App crashes on startup"
- Check device supports OpenGL ES 3.0
- Verify native library is loaded: `adb logcat | grep TX_JNI`

#### "Black screen"
- Check EGL initialization in logs
- Verify surface is created properly

#### "Keyboard not appearing"
- Ensure TerminalSurface has focus
- Check soft keyboard settings in device

### Performance Issues

#### "Slow rendering"
- Enable GPU profiling in Developer Options
- Check for excessive redraws

#### "High memory usage"
- Use Memory Profiler in Android Studio
- Check for memory leaks in native code

## IDE Configuration

### Android Studio Settings

Recommended settings for TX Terminal development:

```
Editor → Code Style → Kotlin
  - Set from: Kotlin style guide

Editor → Inspections
  - Enable: Android, Kotlin, C++

Build → Build Tools → Gradle
  - Gradle JDK: JDK 17

Languages & Frameworks → Android SDK
  - SDK Platform: Android 14.0
  - NDK: 25.2.9519653
```

### Code Style

Apply project code style:
```bash
# Kotlin
./gradlew ktlintFormat

# C++ (requires clang-format)
find src include -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
```

## Additional Tools

### ADB Commands

```bash
# Install APK
adb install app/build/outputs/apk/debug/app-debug.apk

# View logs
adb logcat -s TX_JNI:D TX_PTY:D

# Shell access
adb shell

# Screenshot
adb shell screencap -p /sdcard/screen.png
adb pull /sdcard/screen.png
```

### Profiling

```bash
# CPU profiling
./gradlew assembleDebug
# Use Android Studio Profiler

# Memory profiling
# Run app with profiling enabled
```

## Next Steps

1. Read [ARCHITECTURE.md](ARCHITECTURE.md) for code structure
2. Read [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines
3. Check [TODO.md](TODO.md) for planned features

## Support

- GitHub Issues: [github.com/yourusername/tx/issues](https://github.com/yourusername/tx/issues)
- Discussions: [github.com/yourusername/tx/discussions](https://github.com/yourusername/tx/discussions)

---

**Happy Coding!** 🚀
