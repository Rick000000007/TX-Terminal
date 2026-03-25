#include <unistd.h>
#include "tx/renderer.hpp"
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define LOG_TAG "TX_Renderer"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace tx {

// Android-specific renderer implementation
// This file provides Android-specific overrides for the renderer

// Stub implementations for Android font handling
// In a full implementation, these would use Android's font APIs

class AndroidFontLoader {
public:
    static bool loadSystemFont(float size, void** out_face) {
        // Try to load system monospace font
        // On Android, we can use ASystemFont or load from /system/fonts/
        
        const char* font_paths[] = {
            "/system/fonts/DroidSansMono.ttf",
            "/system/fonts/RobotoMono-Regular.ttf",
            "/system/fonts/CutiveMono.ttf",
            nullptr
        };
        
        for (int i = 0; font_paths[i]; ++i) {
            if (access(font_paths[i], R_OK) == 0) {
                LOGD("Loading font: %s", font_paths[i]);
                // TODO: Load with FreeType
                return true;
            }
        }
        
        return false;
    }
};

} // namespace tx
