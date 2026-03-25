#include <unistd.h>
#include "tx/pty.hpp"
#include <android/log.h>
#include <sys/system_properties.h>

#define LOG_TAG "TX_PTY_Android"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace tx {

// Android-specific PTY implementation
// Most functionality is in the main pty.cpp, but we add Android-specific helpers here

std::string getAndroidShell() {
    // Check for preferred shell
    const char* shell = getenv("SHELL");
    if (shell && access(shell, X_OK) == 0) {
        return shell;
    }
    
    // Try common Android shells
    const char* shells[] = {
        "/system/bin/sh",
        "/system/xbin/bash",
        "/data/data/com.termux/files/usr/bin/bash",
        "/sbin/sh",
        nullptr
    };
    
    for (int i = 0; shells[i]; ++i) {
        if (access(shells[i], X_OK) == 0) {
            LOGD("Using shell: %s", shells[i]);
            return shells[i];
        }
    }
    
    return "/system/bin/sh";  // Fallback
}

bool setupAndroidEnvironment() {
    // Set Android-specific environment variables
    setenv("ANDROID_ROOT", "/system", 1);
    setenv("ANDROID_DATA", "/data", 1);
    
    // Get Android version
    char sdk_version[PROP_VALUE_MAX];
    __system_property_get("ro.build.version.sdk", sdk_version);
    setenv("ANDROID_SDK", sdk_version, 1);
    
    // Set PATH if not set
    const char* path = getenv("PATH");
    if (!path || strlen(path) == 0) {
        setenv("PATH", "/system/bin:/system/xbin:/vendor/bin", 1);
    }
    
    return true;
}

} // namespace tx
