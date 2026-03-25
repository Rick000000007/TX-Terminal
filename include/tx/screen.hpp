#pragma once

#include "common.hpp"
#include <vector>
#include <deque>

namespace tx {

// Cell attributes - packed for cache efficiency on aarch64
struct Attributes {
    uint32_t fg_color : 24;      // RGB foreground
    uint32_t bg_color : 24;      // RGB background  
    uint32_t bold : 1;
    uint32_t italic : 1;
    uint32_t underline : 1;
    uint32_t double_underline : 1;
    uint32_t strikethrough : 1;
    uint32_t overline : 1;
    uint32_t inverse : 1;
    uint32_t blink : 1;
    uint32_t invisible : 1;
    uint32_t wide : 1;           // Wide character (CJK)
    uint32_t wide_continuation : 1;  // Second half of wide char
    uint32_t protected_cell : 1;
    uint32_t hyperlink : 1;
    uint32_t reserved : 1;
    
    bool operator==(const Attributes& other) const {
        return fg_color == other.fg_color &&
               bg_color == other.bg_color &&
               bold == other.bold &&
               italic == other.italic &&
               underline == other.underline &&
               strikethrough == other.strikethrough &&
               inverse == other.inverse &&
               blink == other.blink &&
               wide == other.wide;
    }
    
    bool operator!=(const Attributes& other) const { return !(*this == other); }
};

static_assert(sizeof(Attributes) == 12, "Attributes should be 12 bytes");

// Single terminal cell
struct Cell {
    char32_t codepoint = 0;      // Unicode codepoint (0 = empty)
    Attributes attrs{};
    
    bool empty() const { return codepoint == 0; }
    void clear() { codepoint = 0; attrs = {}; }
    void set(char32_t cp, const Attributes& a) { codepoint = cp; attrs = a; }
};

static_assert(sizeof(Cell) == 16, "Cell should be 16 bytes (cache line friendly)");

// Selection (highlighted text)
struct Selection {
    int start_col = -1;
    int start_row = -1;
    int end_col = -1;
    int end_row = -1;
    bool active = false;
    bool rectangular = false;
    
    bool contains(int col, int row) const;
    void clear() { active = false; start_col = start_row = end_col = end_row = -1; }
};

// Scrollback line
struct HistoryLine {
    std::vector<Cell> cells;
    bool wrapped = false;  // Line was wrapped from previous
};

// Terminal screen buffer
class Screen {
public:
    Screen(int cols = 80, int rows = 24);
    ~Screen() = default;
    
    // Resize (preserves content where possible)
    void resize(int cols, int rows);
    
    // Dimensions
    int cols() const { return cols_; }
    int rows() const { return rows_; }
    
    // Cursor operations
    void setCursor(int col, int row);
    void moveCursor(int dcol, int drow);
    void saveCursor();
    void restoreCursor();
    void setCursorVisible(bool visible) { cursor_visible_ = visible; }
    void setCursorStyle(int style) { cursor_style_ = style; }
    void setCursorBlink(bool blink) { cursor_blink_ = blink; cursor_blink_state_ = true; cursor_blink_time_ = 0; }
    void setCursorColor(Color color) { cursor_color_ = color; }

    int cursorCol() const { return cursor_col_; }
    int cursorRow() const { return cursor_row_; }
    bool cursorVisible() const { return cursor_visible_; }
    int cursorStyle() const { return cursor_style_; }
    bool cursorBlink() const { return cursor_blink_; }
    bool cursorBlinkState() const { return cursor_blink_state_; }
    Color cursorColor() const { return cursor_color_; }

    // Update cursor blink state (call regularly, e.g., every frame)
    void updateCursorBlink(uint64_t current_time_ms);
    
    // Writing
    void write(char32_t codepoint);
    void write(const char* utf8, size_t len);
    void writeString(std::string_view str);
    
    // Control functions
    void lineFeed();             // LF - move down, scroll if needed
    void carriageReturn();       // CR - move to column 0
    void backspace();            // BS - move left
    void horizontalTab();        // HT - move to next tab stop
    void reverseLineFeed();      // RI - move up, scroll if needed
    void index();                // IND - same as LF
    void nextLine();             // NEL - CR + LF
    
    // Erasing
    void eraseInDisplay(int mode);   // 0=below, 1=above, 2=all, 3=scrollback
    void eraseInLine(int mode);      // 0=right, 1=left, 2=all
    void eraseCharacters(int n);     // Erase n characters at cursor
    void deleteCharacters(int n);    // Delete n characters (shift left)
    void insertCharacters(int n);    // Insert n spaces (shift right)
    void insertLines(int n);         // Insert n lines at cursor row
    void deleteLines(int n);         // Delete n lines at cursor row
    
    // Scrolling
    void scrollUp(int lines = 1);
    void scrollDown(int lines = 1);
    void setScrollRegion(int top, int bottom);
    void resetScrollRegion();
    
    // Tab stops
    void setTabStop();
    void clearTabStop();
    void clearAllTabStops();
    
    // Attributes
    void setForeground(Color color);
    void setBackground(Color color);
    void resetAttributes();
    void setAttribute(int attr, bool on);
    
    // Modes
    void setInsertMode(bool insert) { insert_mode_ = insert; }
    void setAutoWrap(bool wrap) { auto_wrap_ = wrap; }
    void setOriginMode(bool origin) { origin_mode_ = origin; }
    void setApplicationKeypad(bool app) { application_keypad_ = app; }
    void setApplicationCursor(bool app) { application_cursor_ = app; }
    void setBracketedPaste(bool bracket) { bracketed_paste_ = bracket; }
    void setMouseMode(int mode) { mouse_mode_ = mode; }
    
    // Cell access
    const Cell* getRow(int row) const;
    Cell* getRow(int row);
    const Cell& getCell(int col, int row) const;
    Cell& getCell(int col, int row);
    
    // History/scrollback
    const std::deque<HistoryLine>& getHistory() const { return history_; }
    void clearHistory();
    int getHistorySize() const { return static_cast<int>(history_.size()); }
    
    // Selection
    void setSelection(int start_col, int start_row, int end_col, int end_row);
    void clearSelection() { selection_.clear(); }
    const Selection& getSelection() const { return selection_; }
    std::string getSelectedText() const;
    
    // Damage tracking (for efficient rendering)
    struct Damage {
        int start_row;
        int end_row;
        bool full_screen = false;
        
        void addRow(int row);
        void addRange(int start, int end);
        void clear() { start_row = end_row = -1; full_screen = false; }
        bool isDirty() const { return full_screen || start_row >= 0; }
    };
    
    const Damage& getDamage() const { return damage_; }
    void clearDamage() { damage_.clear(); }
    
    // Reset to initial state
    void reset();
    
    // Debug
    void dump() const;
    
private:
    int cols_;
    int rows_;
    
    // Main buffer - row-major order
    std::vector<Cell> buffer_;
    
    // Scrollback history
    std::deque<HistoryLine> history_;
    static constexpr int MAX_HISTORY = 10000;
    
    // Cursor state
    int cursor_col_ = 0;
    int cursor_row_ = 0;
    int saved_cursor_col_ = 0;
    int saved_cursor_row_ = 0;
    bool cursor_visible_ = true;
    int cursor_style_ = 0;  // 0=block, 1=underline, 2=bar
    bool cursor_blink_ = true;  // Enable cursor blinking
    bool cursor_blink_state_ = true;  // Current blink state (visible/hidden)
    uint64_t cursor_blink_time_ = 0;  // Last blink time
    static constexpr uint64_t CURSOR_BLINK_INTERVAL_MS = 530;  // Blink interval
    Color cursor_color_ = colors::BRIGHT_WHITE;
    
    // Current attributes
    Attributes current_attrs_;
    
    // Scroll region
    int scroll_top_ = 0;
    int scroll_bottom_ = 0;
    
    // Tab stops
    std::vector<bool> tab_stops_;
    
    // Modes
    bool insert_mode_ = false;
    bool auto_wrap_ = true;
    bool origin_mode_ = false;
    bool application_keypad_ = false;
    bool application_cursor_ = false;
    bool bracketed_paste_ = false;
    int mouse_mode_ = 0;  // 0=off, 1=x10, 2=normal, 3=button-event, 4=any-event
    
    // Selection
    Selection selection_;
    
    // Damage tracking
    Damage damage_;
    
    // Helpers
    void scrollUpInRegion(int lines);
    void scrollDownInRegion(int lines);
    void pushToHistory(const std::vector<Cell>& line, bool wrapped);
    void adjustCursorForResize(int old_cols, int old_rows);
    int effectiveRow(int row) const;  // Apply origin mode
    void markDamage(int row);
    void markRangeDamage(int start_row, int end_row);
};

} // namespace tx
