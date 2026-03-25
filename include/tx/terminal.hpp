#pragma once

#include "common.hpp"
#include "pty.hpp"
#include "parser.hpp"
#include "screen.hpp"
#include "renderer.hpp"

namespace tx {

// Terminal configuration
struct TerminalConfig {
    int cols = 80;
    int rows = 24;
    std::string shell = "/bin/bash";
    std::vector<std::string> env;
    RenderConfig render;
};

// Main terminal class - orchestrates PTY, parser, screen, and renderer
class Terminal {
public:
    Terminal();
    ~Terminal();
    
    // Non-copyable
    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    
    // Initialize
    bool initialize(const TerminalConfig& config);
    void shutdown();
    
    // Process I/O (call regularly)
    void update();
    
    // Render
    void render();
    
    // Input handling
    void onKey(int key, int mods, bool pressed);
    void onChar(char32_t codepoint);
    void onMouseMove(int x, int y);
    void onMouseButton(int button, bool pressed, int mods);
    void onMouseScroll(int delta);
    void onResize(int width, int height);
    
    // Text input
    void sendText(std::string_view text);
    void sendKey(const std::string& sequence);
    
    // Screen access
    Screen& getScreen() { return screen_; }
    const Screen& getScreen() const { return screen_; }
    
    // PTY access
    PTY& getPTY() { return pty_; }
    
    // Renderer access
    Renderer& getRenderer() { return renderer_; }
    
    // State
    bool isRunning() const { return running_; }
    void setRunning(bool running) { running_ = running; }
    
    // Configuration
    const TerminalConfig& getConfig() const { return config_; }
    
    // Clipboard integration
    void copySelection();
    void pasteText(const std::string& text);
    
    // Search
    void startSearch(const std::string& query);
    void findNext();
    void findPrevious();
    void endSearch();
    
    // Scrolling
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);
    void scrollToTop();
    void scrollToBottom();
    bool isScrolled() const { return scroll_offset_ > 0; }
    
    // Selection
    void startSelection(int col, int row);
    void updateSelection(int col, int row);
    void endSelection();
    void selectWord(int col, int row);
    void selectLine(int row);
    void selectAll();
    
private:
    TerminalConfig config_;
    
    PTY pty_;
    Parser parser_;
    Screen screen_;
    Renderer renderer_;
    
    bool running_ = false;
    bool initialized_ = false;
    
    // Scroll state
    int scroll_offset_ = 0;
    
    // Search state
    std::string search_query_;
    std::vector<std::pair<int, int>> search_results_;
    int current_search_result_ = -1;
    
    // Parser actions
    ParserActions createParserActions();
    
    // Key sequence generation
    std::string keyToSequence(int key, int mods);
    std::string cursorKey(int key, bool application);
    std::string keypadKey(int key, bool application);
    
    // Mouse reporting
    void sendMouseEvent(int button, int x, int y, bool release);
    
    // Resize handling
    void handleResize(int width, int height);
};

} // namespace tx
