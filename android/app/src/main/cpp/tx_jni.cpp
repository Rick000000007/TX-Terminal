#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <sys/stat.h>

#include "tx/terminal.hpp"
#include "tx/config.hpp"

#define LOG_TAG "TX_JNI"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

using namespace tx;

// Terminal instance with EGL context
struct TerminalInstance {
    std::unique_ptr<Terminal> terminal;
    std::thread reader_thread;
    std::atomic<bool> running{false};
    std::atomic<bool> should_exit{false};
    
    // EGL
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLContext egl_context = EGL_NO_CONTEXT;
    ANativeWindow* native_window = nullptr;
    
    // Thread safety
    std::mutex render_mutex;
    std::mutex input_mutex;
    
    // Input queue
    std::queue<std::string> input_queue;
    
    // Exit code
    std::atomic<int> exit_code{-1};
    
    ~TerminalInstance() {
        cleanup();
    }
    
    void cleanup() {
        running = false;
        should_exit = true;
        
        if (reader_thread.joinable()) {
            reader_thread.join();
        }
        
        if (terminal) {
            terminal->shutdown();
            terminal.reset();
        }
        
        cleanupEGL();
    }
    
    void cleanupEGL() {
        if (egl_surface != EGL_NO_SURFACE) {
            eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(egl_display, egl_surface);
            egl_surface = EGL_NO_SURFACE;
        }
        
        if (egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(egl_display, egl_context);
            egl_context = EGL_NO_CONTEXT;
        }
        
        if (egl_display != EGL_NO_DISPLAY) {
            eglTerminate(egl_display);
            egl_display = EGL_NO_DISPLAY;
        }
        
        if (native_window) {
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
    }
    
    bool initializeEGL(ANativeWindow* window) {
        native_window = window;
        ANativeWindow_acquire(native_window);
        
        // Initialize EGL
        egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (egl_display == EGL_NO_DISPLAY) {
            LOGE("Failed to get EGL display");
            return false;
        }
        
        EGLint major, minor;
        if (!eglInitialize(egl_display, &major, &minor)) {
            LOGE("Failed to initialize EGL");
            return false;
        }
        
        LOGD("EGL version: %d.%d", major, minor);
        
        // Choose config
        const EGLint config_attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 0,
            EGL_STENCIL_SIZE, 0,
            EGL_NONE
        };
        
        EGLConfig config;
        EGLint num_configs;
        if (!eglChooseConfig(egl_display, config_attribs, &config, 1, &num_configs)) {
            LOGE("Failed to choose EGL config");
            return false;
        }
        
        // Create context
        const EGLint context_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };
        
        egl_context = eglCreateContext(egl_display, config, EGL_NO_CONTEXT, context_attribs);
        if (egl_context == EGL_NO_CONTEXT) {
            LOGE("Failed to create EGL context");
            return false;
        }
        
        // Create surface
        egl_surface = eglCreateWindowSurface(egl_display, config, native_window, nullptr);
        if (egl_surface == EGL_NO_SURFACE) {
            LOGE("Failed to create EGL surface");
            return false;
        }
        
        // Make current
        eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
        
        LOGD("EGL initialized successfully");
        return true;
    }
    
    void render() {
        std::lock_guard<std::mutex> lock(render_mutex);
        
        if (egl_surface == EGL_NO_SURFACE || !terminal) {
            return;
        }
        
        eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);
        terminal->render();
        eglSwapBuffers(egl_display, egl_surface);
    }
};

// Global map of terminal instances
static std::mutex instances_mutex;
static std::unordered_map<jlong, std::unique_ptr<TerminalInstance>> instances;
static jlong next_handle = 1;

// Helper to ensure directory exists
static bool ensureDirectoryExists(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    // Try to create
    if (mkdir(path.c_str(), 0755) == 0) {
        LOGD("Created directory: %s", path.c_str());
        return true;
    }
    return false;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_tx_terminal_jni_NativeTerminal_create(
    JNIEnv* env,
    jclass clazz,
    jint columns,
    jint rows,
    jstring shell_path,
    jstring initial_command,
    jstring working_directory,
    jobjectArray env_vars
) {
    LOGD("Creating terminal: %dx%d", columns, rows);
    
    auto instance = std::make_unique<TerminalInstance>();
    
    // Get shell path
    const char* shell_str = env->GetStringUTFChars(shell_path, nullptr);
    std::string shell(shell_str);
    env->ReleaseStringUTFChars(shell_path, shell_str);
    
    // Get working directory
    std::string cwd;
    if (working_directory) {
        const char* cwd_str = env->GetStringUTFChars(working_directory, nullptr);
        cwd = cwd_str;
        env->ReleaseStringUTFChars(working_directory, cwd_str);
        LOGD("Working directory: %s", cwd.c_str());
        
        // Ensure working directory exists
        if (!cwd.empty() && !ensureDirectoryExists(cwd)) {
            LOGW("Working directory doesn't exist and couldn't be created: %s", cwd.c_str());
            // Will fall back in PTY::open
        }
    }
    
    // Get initial command if provided
    std::string init_cmd;
    if (initial_command) {
        const char* cmd_str = env->GetStringUTFChars(initial_command, nullptr);
        init_cmd = cmd_str;
        env->ReleaseStringUTFChars(initial_command, cmd_str);
    }
    
    // Configure terminal
    TerminalConfig config;
    config.cols = columns;
    config.rows = rows;
    config.shell = shell;
    config.cwd = cwd;  // Set working directory
    
    // Build environment variables
    // Start with defaults
    config.env.push_back("TERM=xterm-256color");
    config.env.push_back("COLORTERM=truecolor");
    config.env.push_back("ANDROID=1");
    config.env.push_back("ANDROID_ROOT=/system");
    config.env.push_back("ANDROID_DATA=/data");
    
    // Add custom environment variables from Java
    if (env_vars) {
        jsize env_count = env->GetArrayLength(env_vars);
        for (jsize i = 0; i < env_count; i++) {
            jstring env_var = (jstring)env->GetObjectArrayElement(env_vars, i);
            if (env_var) {
                const char* var_str = env->GetStringUTFChars(env_var, nullptr);
                if (var_str) {
                    config.env.push_back(var_str);
                    LOGD("Added env var: %s", var_str);
                }
                env->ReleaseStringUTFChars(env_var, var_str);
                env->DeleteLocalRef(env_var);
            }
        }
    }
    
    // Create terminal
    instance->terminal = std::make_unique<Terminal>();
    
    if (!instance->terminal->initialize(config)) {
        LOGE("Failed to initialize terminal");
        return 0;
    }
    
    instance->running = true;
    
    // Start reader thread
    instance->reader_thread = std::thread([instance_ptr = instance.get()]() {
        while (instance_ptr->running && !instance_ptr->should_exit) {
            if (instance_ptr->terminal) {
                instance_ptr->terminal->update();
                
                // Check if process exited
                if (!instance_ptr->terminal->isRunning()) {
                    instance_ptr->exit_code = 0;
                    instance_ptr->running = false;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Store instance
    jlong handle = next_handle++;
    {
        std::lock_guard<std::mutex> lock(instances_mutex);
        instances[handle] = std::move(instance);
    }
    
    LOGD("Terminal created with handle: %ld", handle);
    return handle;
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_destroy(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    LOGD("Destroying terminal: %ld", handle);
    
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end()) {
        it->second->cleanup();
        instances.erase(it);
    }
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_setSurface(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jobject surface
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end()) {
        LOGE("Invalid handle: %ld", handle);
        return;
    }
    
    auto& instance = it->second;
    
    // Get native window
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get native window");
        return;
    }
    
    // Initialize EGL
    if (!instance->initializeEGL(window)) {
        ANativeWindow_release(window);
        return;
    }
    
    // Get window size and resize terminal
    int width = ANativeWindow_getWidth(window);
    int height = ANativeWindow_getHeight(window);
    
    if (instance->terminal) {
        if (!instance->terminal->initializeRenderer(width, height)) {
            LOGE("Failed to initialize renderer after EGL setup");
            instance->cleanupEGL();
            return;
        }
    }
    
    LOGD("Surface set: %dx%d", width, height);
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_destroySurface(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end()) {
        it->second->cleanupEGL();
    }
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_render(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end()) {
        it->second->render();
    }
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_resize(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint columns,
    jint rows
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        it->second->terminal->resizeTerminal(columns, rows);
    }
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_sendKey(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint key_code,
    jint modifiers,
    jboolean pressed
) {
    if (!pressed) return;  // Only handle key down for now
    
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end() || !it->second->terminal) {
        return;
    }
    
    auto& terminal = it->second->terminal;
    
    // Convert Android key code to terminal key
    int mods = 0;
    if (modifiers & 1) mods |= 1;  // Shift
    if (modifiers & 2) mods |= 2;  // Ctrl
    if (modifiers & 4) mods |= 4;  // Alt
    if (modifiers & 8) mods |= 8;  // Meta
    
    terminal->onKey(key_code, mods, true);
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_sendChar(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint codepoint
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        it->second->terminal->onChar(static_cast<char32_t>(codepoint));
    }
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_sendText(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jstring text
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end() || !it->second->terminal) {
        return;
    }
    
    const char* text_str = env->GetStringUTFChars(text, nullptr);
    std::string input(text_str);
    env->ReleaseStringUTFChars(text, text_str);
    
    it->second->terminal->sendText(input);
}

JNIEXPORT jstring JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getScreenContent(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end() || !it->second->terminal) {
        return env->NewStringUTF("");
    }
    
    // Get screen content
    const auto& screen = it->second->terminal->getScreen();
    std::string content;
    
    for (int row = 0; row < screen.rows(); ++row) {
        const auto* line = screen.getRow(row);
        if (!line) continue;
        
        for (int col = 0; col < screen.cols(); ++col) {
            if (!line[col].empty()) {
                char32_t cp = line[col].codepoint;
                // Convert to UTF-8
                if (cp < 0x80) {
                    content += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    content += static_cast<char>(0xC0 | (cp >> 6));
                    content += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    content += static_cast<char>(0xE0 | (cp >> 12));
                    content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    content += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    content += static_cast<char>(0xF0 | (cp >> 18));
                    content += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    content += static_cast<char>(0x80 | (cp & 0x3F));
                }
            }
        }
        content += '\n';
    }
    
    return env->NewStringUTF(content.c_str());
}


JNIEXPORT jstring JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getRowText(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint row
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end() || !it->second->terminal) {
        return env->NewStringUTF("");
    }

    const auto& screen = it->second->terminal->getScreen();
    if (row < 0 || row >= screen.rows()) {
        return env->NewStringUTF("");
    }

    const auto* line = screen.getRow(row);
    if (!line) {
        return env->NewStringUTF("");
    }

    std::string content;
    for (int col = 0; col < screen.cols(); ++col) {
        if (!line[col].empty()) {
            char32_t cp = line[col].codepoint;
            if (cp < 0x80) {
                content += static_cast<char>(cp);
            } else if (cp < 0x800) {
                content += static_cast<char>(0xC0 | (cp >> 6));
                content += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                content += static_cast<char>(0xE0 | (cp >> 12));
                content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                content += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                content += static_cast<char>(0xF0 | (cp >> 18));
                content += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                content += static_cast<char>(0x80 | (cp & 0x3F));
            }
        } else {
            content += ' ';
        }
    }

    return env->NewStringUTF(content.c_str());
}

JNIEXPORT jint JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getHistorySize(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end() || !it->second->terminal) {
        return 0;
    }

    return it->second->terminal->getScreen().getHistorySize();
}

JNIEXPORT jstring JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getHistoryRowText(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint row
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end() || !it->second->terminal) {
        return env->NewStringUTF("");
    }

    const auto& screen = it->second->terminal->getScreen();
    const auto& history = screen.getHistory();

    if (row < 0 || row >= static_cast<jint>(history.size())) {
        return env->NewStringUTF("");
    }

    const auto& line = history[row].cells;
    std::string content;

    for (int col = 0; col < screen.cols(); ++col) {
        if (col < static_cast<int>(line.size()) && !line[col].empty()) {
            char32_t cp = line[col].codepoint;
            if (cp < 0x80) {
                content += static_cast<char>(cp);
            } else if (cp < 0x800) {
                content += static_cast<char>(0xC0 | (cp >> 6));
                content += static_cast<char>(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                content += static_cast<char>(0xE0 | (cp >> 12));
                content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                content += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                content += static_cast<char>(0xF0 | (cp >> 18));
                content += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                content += static_cast<char>(0x80 | (cp & 0x3F));
            }
        } else {
            content += ' ';
        }
    }

    return env->NewStringUTF(content.c_str());
}

JNIEXPORT jobjectArray JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getVisibleRowsText(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint firstVisibleRow,
    jint rowCount
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it == instances.end() || !it->second->terminal || rowCount <= 0) {
        jclass stringClass = env->FindClass("java/lang/String");
        return env->NewObjectArray(0, stringClass, env->NewStringUTF(""));
    }

    const auto& screen = it->second->terminal->getScreen();
    const auto& history = screen.getHistory();
    const int historySize = static_cast<int>(history.size());
    const int screenRows = screen.rows();
    const int totalRows = historySize + screenRows;

    int startRow = std::max(0, static_cast<int>(firstVisibleRow));
    int count = std::max(0, static_cast<int>(rowCount));
    if (startRow > totalRows) startRow = totalRows;
    if (startRow + count > totalRows) count = totalRows - startRow;

    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(count, stringClass, env->NewStringUTF(""));

    auto append_utf8 = [](std::string& content, char32_t cp) {
        if (cp < 0x80) {
            content += static_cast<char>(cp);
        } else if (cp < 0x800) {
            content += static_cast<char>(0xC0 | (cp >> 6));
            content += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            content += static_cast<char>(0xE0 | (cp >> 12));
            content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            content += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            content += static_cast<char>(0xF0 | (cp >> 18));
            content += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            content += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            content += static_cast<char>(0x80 | (cp & 0x3F));
        }
    };

    for (int i = 0; i < count; ++i) {
        int virtualRow = startRow + i;
        std::string content;
        content.reserve(screen.cols());

        if (virtualRow < historySize) {
            const auto& line = history[virtualRow].cells;
            for (int col = 0; col < screen.cols(); ++col) {
                if (col < static_cast<int>(line.size()) && !line[col].empty()) {
                    append_utf8(content, line[col].codepoint);
                } else {
                    content += ' ';
                }
            }
        } else {
            int row = virtualRow - historySize;
            const auto* line = screen.getRow(row);
            if (line != nullptr) {
                for (int col = 0; col < screen.cols(); ++col) {
                    if (!line[col].empty()) {
                        append_utf8(content, line[col].codepoint);
                    } else {
                        content += ' ';
                    }
                }
            }
        }

        env->SetObjectArrayElement(result, i, env->NewStringUTF(content.c_str()));
    }

    return result;
}

JNIEXPORT jint JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getColumns(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        return it->second->terminal->getScreen().cols();
    }
    return 80;
}

JNIEXPORT jint JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getRows(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        return it->second->terminal->getScreen().rows();
    }
    return 24;
}

JNIEXPORT jint JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getCursorCol(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        return it->second->terminal->getScreen().cursorCol();
    }
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getCursorRow(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        return it->second->terminal->getScreen().cursorRow();
    }
    return 0;
}

JNIEXPORT jboolean JNICALL
Java_com_tx_terminal_jni_NativeTerminal_isRunning(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end()) {
        return it->second->running.load() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getExitCode(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end()) {
        return it->second->exit_code.load();
    }
    return -1;
}

JNIEXPORT jstring JNICALL
Java_com_tx_terminal_jni_NativeTerminal_copySelection(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        std::string text = it->second->terminal->getScreen().getSelectedText();
        return env->NewStringUTF(text.c_str());
    }
    return env->NewStringUTF("");
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_setSelection(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint start_col,
    jint start_row,
    jint end_col,
    jint end_row
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        it->second->terminal->getScreen().setSelection(start_col, start_row, end_col, end_row);
    }
}

JNIEXPORT void JNICALL
Java_com_tx_terminal_jni_NativeTerminal_clearSelection(
    JNIEnv* env,
    jclass clazz,
    jlong handle
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        it->second->terminal->getScreen().clearSelection();
    }
}

JNIEXPORT jboolean JNICALL
Java_com_tx_terminal_jni_NativeTerminal_isCellSelected(
    JNIEnv* env,
    jclass clazz,
    jlong handle,
    jint col,
    jint row
) {
    std::lock_guard<std::mutex> lock(instances_mutex);
    auto it = instances.find(handle);
    if (it != instances.end() && it->second->terminal) {
        return it->second->terminal->getScreen().isCellSelected(col, row) ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_tx_terminal_jni_NativeTerminal_getVersion(
    JNIEnv* env,
    jclass clazz
) {
    return env->NewStringUTF(tx::VERSION);
}

} // extern "C"
