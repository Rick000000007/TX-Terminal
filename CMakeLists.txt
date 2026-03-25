cmake_minimum_required(VERSION 3.22)
project(TX VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(TX_BUILD_TESTS "Build tests" ON)
option(TX_BUILD_ANDROID "Build for Android" OFF)

# Platform detection
if(ANDROID)
    set(TX_BUILD_ANDROID ON)
    message(STATUS "Building for Android (${ANDROID_ABI})")
endif()

# Find packages
if(NOT ANDROID)
    find_package(PkgConfig REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(Freetype REQUIRED)
    pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
endif()

# Core library sources
set(TX_CORE_SOURCES
    src/pty.cpp
    src/parser.cpp
    src/screen.cpp
    src/renderer.cpp
    src/terminal.cpp
    src/config.cpp
)

set(TX_CORE_HEADERS
    include/tx/pty.hpp
    include/tx/parser.hpp
    include/tx/screen.hpp
    include/tx/renderer.hpp
    include/tx/terminal.hpp
    include/tx/config.hpp
    include/tx/common.hpp
)

# Create core library
add_library(tx_core STATIC ${TX_CORE_SOURCES})

target_include_directories(tx_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(NOT ANDROID)
    target_include_directories(tx_core PRIVATE
        ${FREETYPE_INCLUDE_DIRS}
        ${HARFBUZZ_INCLUDE_DIRS}
    )
endif()

# Platform-specific settings
if(ANDROID)
    # Android-specific settings
    target_compile_definitions(tx_core PRIVATE TX_ANDROID=1)
    
    # Link Android libraries
    target_link_libraries(tx_core PRIVATE
        log
        android
        EGL
        GLESv3
    )
else()
    # Desktop Linux settings
    target_compile_definitions(tx_core PRIVATE TX_DESKTOP=1)
    
    target_link_libraries(tx_core PRIVATE
        ${FREETYPE_LIBRARIES}
        ${HARFBUZZ_LIBRARIES}
        util
    )
endif()

# Compiler optimizations for aarch64
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    target_compile_options(tx_core PRIVATE
        -march=armv8-a+fp+simd+crc+crypto
        -O3
        -ffast-math
        -flto
    )
    target_link_options(tx_core PRIVATE -flto)
    
    # Enable NEON optimizations
    target_compile_definitions(tx_core PRIVATE TX_USE_NEON=1)
endif()

# Enable warnings
target_compile_options(tx_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
)

# Desktop executable
if(NOT ANDROID)
    add_executable(tx src/main_desktop.cpp)
    target_link_libraries(tx PRIVATE
        tx_core
        OpenGL::GL
        glfw
    )
    
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
        target_compile_options(tx PRIVATE
            -march=armv8-a+fp+simd+crc+crypto
            -O3
            -ffast-math
            -flto
        )
        target_link_options(tx PRIVATE -flto)
    endif()
endif()

# Tests
if(TX_BUILD_TESTS AND NOT ANDROID)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
if(NOT ANDROID)
    install(TARGETS tx DESTINATION bin)
    install(DIRECTORY include/tx DESTINATION include)
endif()
