cmake_minimum_required(VERSION 3.22)
project(tx_android VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 🔥 Resolve project root correctly (CRITICAL FIX)
get_filename_component(TX_ROOT ${CMAKE_SOURCE_DIR}/../../../../../ ABSOLUTE)

# Android system libraries
find_library(ANDROID_LOG_LIB log)
find_library(ANDROID_LIB android)
find_library(EGL_LIB EGL)
find_library(GLES_LIB GLESv3)

# Include directories (modern + safe)
include_directories(
    ${TX_ROOT}/include
    ${CMAKE_SOURCE_DIR}
)

# 🔥 Core engine sources
set(TX_CORE_SOURCES
    ${TX_ROOT}/src/pty.cpp
    ${TX_ROOT}/src/parser.cpp
    ${TX_ROOT}/src/screen.cpp
    ${TX_ROOT}/src/renderer.cpp
    ${TX_ROOT}/src/terminal.cpp
    ${TX_ROOT}/src/config.cpp
)

# 🔥 Core static library
add_library(tx_core STATIC ${TX_CORE_SOURCES})

target_compile_definitions(tx_core PRIVATE
    TX_ANDROID=1
    ANDROID=1
)

target_include_directories(tx_core PRIVATE
    ${TX_ROOT}/include
)

target_link_libraries(tx_core PRIVATE
    ${ANDROID_LOG_LIB}
    ${ANDROID_LIB}
    ${EGL_LIB}
    ${GLES_LIB}
)

# 🔥 ARM64 optimizations (safe tuned)
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    target_compile_options(tx_core PRIVATE
        -march=armv8-a
        -O3
        -ffast-math
    )
    target_compile_definitions(tx_core PRIVATE TX_USE_NEON=1)
endif()

# 🔥 JNI bridge
add_library(tx_jni SHARED
    tx_jni.cpp
    android_renderer.cpp
    android_pty.cpp
)

target_include_directories(tx_jni PRIVATE
    ${TX_ROOT}/include
)

target_link_libraries(tx_jni PRIVATE
    tx_core
    ${ANDROID_LOG_LIB}
    ${ANDROID_LIB}
    ${EGL_LIB}
    ${GLES_LIB}
)

# 🔥 JNI optimizations
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    target_compile_options(tx_jni PRIVATE
        -march=armv8-a
        -O3
        -ffast-math
    )
endif()

# 🔥 STL (required for stability)
set(CMAKE_ANDROID_STL_TYPE c++_shared)
