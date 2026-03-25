#include "tx/config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace tx {

std::unordered_map<std::string, std::string> Config::values_;

bool Config::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    return loadFromString(content);
}

bool Config::loadFromString(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        // Parse key = value
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        // Trim key and value
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        
        // Remove quotes from value
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        
        values_[key] = value;
    }
    
    return true;
}

bool Config::saveToFile(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << "# TX Terminal Configuration\n\n";
    
    for (const auto& [key, value] : values_) {
        file << key << " = ";
        
        // Quote string values if they contain spaces
        if (value.find(' ') != std::string::npos || value.find('\t') != std::string::npos) {
            file << "\"" << value << "\"";
        } else {
            file << value;
        }
        
        file << "\n";
    }
    
    return true;
}

std::string Config::getString(const std::string& key, const std::string& default_value) {
    auto it = values_.find(key);
    return (it != values_.end()) ? it->second : default_value;
}

int Config::getInt(const std::string& key, int default_value) {
    auto it = values_.find(key);
    if (it == values_.end()) return default_value;
    
    try {
        return std::stoi(it->second);
    } catch (...) {
        return default_value;
    }
}

float Config::getFloat(const std::string& key, float default_value) {
    auto it = values_.find(key);
    if (it == values_.end()) return default_value;
    
    try {
        return std::stof(it->second);
    } catch (...) {
        return default_value;
    }
}

bool Config::getBool(const std::string& key, bool default_value) {
    auto it = values_.find(key);
    if (it == values_.end()) return default_value;
    
    std::string val = it->second;
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    
    if (val == "true" || val == "1" || val == "yes" || val == "on") {
        return true;
    } else if (val == "false" || val == "0" || val == "no" || val == "off") {
        return false;
    }
    
    return default_value;
}

Color Config::getColor(const std::string& key, Color default_value) {
    auto it = values_.find(key);
    if (it == values_.end()) return default_value;
    
    return parseColor(it->second);
}

void Config::setString(const std::string& key, const std::string& value) {
    values_[key] = value;
}

void Config::setInt(const std::string& key, int value) {
    values_[key] = std::to_string(value);
}

void Config::setFloat(const std::string& key, float value) {
    values_[key] = std::to_string(value);
}

void Config::setBool(const std::string& key, bool value) {
    values_[key] = value ? "true" : "false";
}

void Config::setColor(const std::string& key, Color value) {
    values_[key] = colorToString(value);
}

void Config::applyTo(TerminalConfig& config) {
    config.cols = getInt("terminal.cols", config.cols);
    config.rows = getInt("terminal.rows", config.rows);
    config.shell = getString("terminal.shell", config.shell);
    
    applyTo(config.render);
}

void Config::applyTo(RenderConfig& config) {
    config.font_size = getFloat("font.size", config.font_size);
    config.font_path = getString("font.path", config.font_path);
    config.use_bold_font = getBool("font.use_bold", config.use_bold_font);
    config.use_italic_font = getBool("font.use_italic", config.use_italic_font);
    config.use_ligatures = getBool("font.use_ligatures", config.use_ligatures);
    config.background_color = getColor("color.background", config.background_color);
}

std::string Config::getDefaultPath() {
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.config/tx/tx.conf";
    }
    return "tx.conf";
}

std::string Config::getSystemPath() {
    return "/etc/tx/tx.conf";
}

void Config::reset() {
    values_.clear();
    loadFromString(defaults::CONFIG_TEXT);
}

Color Config::parseColor(const std::string& str) {
    if (str.empty()) return colors::BLACK;
    
    // Hex color: 0xRRGGBB or #RRGGBB
    if (str.size() == 8 && str.substr(0, 2) == "0x") {
        return static_cast<Color>(std::stoul(str, nullptr, 16));
    }
    
    if (str.size() == 7 && str[0] == '#') {
        return static_cast<Color>(std::stoul(str.substr(1), nullptr, 16));
    }
    
    // Named colors
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "black") return colors::BLACK;
    if (lower == "red") return colors::RED;
    if (lower == "green") return colors::GREEN;
    if (lower == "yellow") return colors::YELLOW;
    if (lower == "blue") return colors::BLUE;
    if (lower == "magenta") return colors::MAGENTA;
    if (lower == "cyan") return colors::CYAN;
    if (lower == "white") return colors::WHITE;
    
    return colors::BLACK;
}

std::string Config::colorToString(Color color) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << (color & 0xFFFFFF);
    return oss.str();
}

} // namespace tx
