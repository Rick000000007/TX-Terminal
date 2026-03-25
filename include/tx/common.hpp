#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <memory>
#include <algorithm>
#include <optional>

// Platform detection
#if defined(__ANDROID__)
    #define TX_ANDROID 1
#elif defined(__linux__)
    #define TX_LINUX 1
#endif

// Architecture detection
#if defined(__aarch64__) || defined(_M_ARM64)
    #define TX_AARCH64 1
    #define TX_USE_NEON 1
#endif

// NEON intrinsics for aarch64
#if defined(TX_USE_NEON)
    #include <arm_neon.h>
#endif

namespace tx {

// Version info
constexpr const char* VERSION = "1.0.0";
constexpr const char* APP_NAME = "TX Terminal";

// Type aliases
using Codepoint = char32_t;
using Color = uint32_t;  // 0xAARRGGBB

// Constants
constexpr int MAX_COLS = 512;
constexpr int MAX_ROWS = 256;
constexpr int SCROLLBACK_LINES = 10000;
constexpr int TAB_STOP = 8;

// Color utilities
constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (static_cast<Color>(0xFF) << 24) |
           (static_cast<Color>(r) << 16) |
           (static_cast<Color>(g) << 8)  |
           static_cast<Color>(b);
}

constexpr Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (static_cast<Color>(a) << 24) |
           (static_cast<Color>(r) << 16) |
           (static_cast<Color>(g) << 8)  |
           static_cast<Color>(b);
}


// Default colors
namespace colors {
    constexpr Color BLACK       = rgb(0x00, 0x00, 0x00);
    constexpr Color RED         = rgb(0xCD, 0x31, 0x31);
    constexpr Color GREEN       = rgb(0x0D, 0xBC, 0x79);
    constexpr Color YELLOW      = rgb(0xE5, 0xE5, 0x10);
    constexpr Color BLUE        = rgb(0x24, 0x72, 0xC8);
    constexpr Color MAGENTA     = rgb(0xBC, 0x3F, 0xBC);
    constexpr Color CYAN        = rgb(0x11, 0xA8, 0xCD);
    constexpr Color WHITE       = rgb(0xE5, 0xE5, 0xE5);
    constexpr Color BRIGHT_BLACK    = rgb(0x66, 0x66, 0x66);
    constexpr Color BRIGHT_RED      = rgb(0xF1, 0x4C, 0x4C);
    constexpr Color BRIGHT_GREEN    = rgb(0x23, 0xD1, 0x8B);
    constexpr Color BRIGHT_YELLOW   = rgb(0xF5, 0xF5, 0x43);
    constexpr Color BRIGHT_BLUE     = rgb(0x3B, 0x8E, 0xEE);
    constexpr Color BRIGHT_MAGENTA  = rgb(0xD6, 0x70, 0xD6);
    constexpr Color BRIGHT_CYAN     = rgb(0x29, 0xB8, 0xDB);
    constexpr Color BRIGHT_WHITE    = rgb(0xFF, 0xFF, 0xFF);
    constexpr Color DEFAULT_FG  = BRIGHT_WHITE;
    constexpr Color DEFAULT_BG  = BLACK;
}

// ANSI 256-color palette (first 16 are standard colors)
inline const std::array<Color, 256>& get_ansi_palette() {
    static std::array<Color, 256> palette;
    static bool initialized = false;
    
    if (!initialized) {
        // Standard 16 colors
        palette[0]  = colors::BLACK;
        palette[1]  = colors::RED;
        palette[2]  = colors::GREEN;
        palette[3]  = colors::YELLOW;
        palette[4]  = colors::BLUE;
        palette[5]  = colors::MAGENTA;
        palette[6]  = colors::CYAN;
        palette[7]  = colors::WHITE;
        palette[8]  = colors::BRIGHT_BLACK;
        palette[9]  = colors::BRIGHT_RED;
        palette[10] = colors::BRIGHT_GREEN;
        palette[11] = colors::BRIGHT_YELLOW;
        palette[12] = colors::BRIGHT_BLUE;
        palette[13] = colors::BRIGHT_MAGENTA;
        palette[14] = colors::BRIGHT_CYAN;
        palette[15] = colors::BRIGHT_WHITE;
        
        // 216 color cube (6x6x6)
        for (int r = 0; r < 6; ++r) {
            for (int g = 0; g < 6; ++g) {
                for (int b = 0; b < 6; ++b) {
                    int idx = 16 + r * 36 + g * 6 + b;
                    uint8_t rv = r ? (r * 40 + 55) : 0;
                    uint8_t gv = g ? (g * 40 + 55) : 0;
                    uint8_t bv = b ? (b * 40 + 55) : 0;
                    palette[idx] = rgb(rv, gv, bv);
                }
            }
        }
        
        // 24 grayscale colors
        for (int i = 0; i < 24; ++i) {
            uint8_t v = i * 10 + 8;
            palette[232 + i] = rgb(v, v, v);
        }
        
        initialized = true;
    }
    
    return palette;
}

// UTF-8 utilities
inline size_t utf8_sequence_length(uint8_t first_byte) {
    if ((first_byte & 0x80) == 0) return 1;
    if ((first_byte & 0xE0) == 0xC0) return 2;
    if ((first_byte & 0xF0) == 0xE0) return 3;
    if ((first_byte & 0xF8) == 0xF0) return 4;
    return 1;  // Invalid, treat as single byte
}

inline char32_t utf8_decode(const char* str, size_t& out_len) {
    uint8_t b0 = static_cast<uint8_t>(str[0]);
    out_len = utf8_sequence_length(b0);
    
    switch (out_len) {
        case 1: return b0;
        case 2: return ((b0 & 0x1F) << 6) | (str[1] & 0x3F);
        case 3: return ((b0 & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
        case 4: return ((b0 & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
        default: return b0;
    }
}

// SIMD-optimized memory operations for aarch64
#if defined(TX_USE_NEON)
inline void neon_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    
    // Copy 64 bytes at a time using NEON
    while (n >= 64) {
        uint8x16x4_t data = vld1q_u8_x4(s);
        vst1q_u8_x4(d, data);
        s += 64;
        d += 64;
        n -= 64;
    }
    
    // Copy remaining bytes
    while (n--) *d++ = *s++;
}

inline void neon_memset(void* dst, uint8_t val, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    uint8x16_t v = vdupq_n_u8(val);
    
    while (n >= 64) {
        vst1q_u8(d, v);
        vst1q_u8(d + 16, v);
        vst1q_u8(d + 32, v);
        vst1q_u8(d + 48, v);
        d += 64;
        n -= 64;
    }
    
    while (n >= 16) {
        vst1q_u8(d, v);
        d += 16;
        n -= 16;
    }
    
    while (n--) *d++ = val;
}
#endif

} // namespace tx
