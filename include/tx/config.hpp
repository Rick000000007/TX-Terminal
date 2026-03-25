#include "tx/terminal.hpp"
#pragma once

#include "common.hpp"
#include "renderer.hpp"
#include <string>

namespace tx {

// Configuration management
class Config {
public:
    // Load from file
    static bool loadFromFile(const std::string& path);
    static bool loadFromString(const std::string& content);
    
    // Save to file
    static bool saveToFile(const std::string& path);
    
    // Get/set values
    static std::string getString(const std::string& key, const std::string& default_value = "");
    static int getInt(const std::string& key, int default_value = 0);
    static float getFloat(const std::string& key, float default_value = 0.0f);
    static bool getBool(const std::string& key, bool default_value = false);
    static Color getColor(const std::string& key, Color default_value);
    
    static void setString(const std::string& key, const std::string& value);
    static void setInt(const std::string& key, int value);
    static void setFloat(const std::string& key, float value);
    static void setBool(const std::string& key, bool value);
    static void setColor(const std::string& key, Color value);
    
    // Apply to terminal config
    static void applyTo(TerminalConfig& config);
    static void applyTo(RenderConfig& config);
    
    // Default config paths
    static std::string getDefaultPath();
    static std::string getSystemPath();
    
    // Reset to defaults
    static void reset();
    
private:
    // Internal storage
    static std::unordered_map<std::string, std::string> values_;
    
    // Helpers
    static Color parseColor(const std::string& str);
    static std::string colorToString(Color color);
};

// Default configuration
namespace defaults {
    constexpr const char* CONFIG_TEXT = R"(
# TX Terminal Configuration

# Font settings
font.size = 14.0
font.path = ""
font.use_bold = true
font.use_italic = true
font.use_ligatures = false

# Colors (0xRRGGBB format)
color.background = 0x000000
color.foreground = 0xFFFFFF
color.cursor = 0xFFFFFF
color.selection = 0x3D3D3D

# ANSI colors
color.black = 0x000000
color.red = 0xCD3131
color.green = 0x0DBC79
color.yellow = 0xE5E510
color.blue = 0x2472C8
color.magenta = 0xBC3FBC
color.cyan = 0x11A8CD
color.white = 0xE5E5E5
color.bright_black = 0x666666
color.bright_red = 0xF14C4C
color.bright_green = 0x23D18B
color.bright_yellow = 0xF5F543
color.bright_blue = 0x3B8EEE
color.bright_magenta = 0xD670D6
color.bright_cyan = 0x29B8DB
color.bright_white = 0xFFFFFF

# Terminal settings
terminal.cols = 80
terminal.rows = 24
terminal.scrollback = 10000
terminal.shell = "/bin/bash"

# Behavior
behavior.cursor_blink = false
behavior.cursor_style = 0  # 0=block, 1=underline, 2=bar
behavior.copy_on_select = true
behavior.scroll_on_output = true
behavior.scroll_on_key = true
)";
}

} // namespace tx
