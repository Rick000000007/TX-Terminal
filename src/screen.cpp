#include <iostream>
#include "tx/screen.hpp"
#include <algorithm>
#include <cstring>

namespace tx {

// Selection helpers
bool Selection::contains(int col, int row) const {
    if (!active) return false;
    
    int start_r = std::min(start_row, end_row);
    int end_r = std::max(start_row, end_row);
    
    if (row < start_r || row > end_r) return false;
    
    if (rectangular) {
        int start_c = std::min(start_col, end_col);
        int end_c = std::max(start_col, end_col);
        return col >= start_c && col <= end_c;
    } else {
        if (row == start_r && row == end_r) {
            int start_c = std::min(start_col, end_col);
            int end_c = std::max(start_col, end_col);
            return col >= start_c && col <= end_c;
        } else if (row == start_r) {
            return col >= (start_row < end_row ? start_col : end_col);
        } else if (row == end_r) {
            return col <= (start_row < end_row ? end_col : start_col);
        }
        return true;
    }
}

// Damage tracking
void Screen::Damage::addRow(int row) {
    if (full_screen) return;
    if (start_row < 0) {
        start_row = end_row = row;
    } else {
        start_row = std::min(start_row, row);
        end_row = std::max(end_row, row);
    }
}

void Screen::Damage::addRange(int start, int end) {
    if (full_screen) return;
    if (start_row < 0) {
        start_row = start;
        end_row = end;
    } else {
        start_row = std::min(start_row, start);
        end_row = std::max(end_row, end);
    }
}

Screen::Screen(int cols, int rows)
    : cols_(cols)
    , rows_(rows)
    , buffer_(cols * rows)
    , tab_stops_(cols, false) {
    
    scroll_bottom_ = rows_ - 1;
    
    // Set default tab stops every 8 columns
    for (int i = TAB_STOP; i < cols_; i += TAB_STOP) {
        tab_stops_[i] = true;
    }
    
    reset();
}

void Screen::resize(int cols, int rows) {
    if (cols == cols_ && rows == rows_) return;
    
    // Save old buffer
    std::vector<Cell> old_buffer = std::move(buffer_);
    int old_cols = cols_;
    int old_rows = rows_;
    
    // Update dimensions
    cols_ = cols;
    rows_ = rows;
    
    // Allocate new buffer
    buffer_.resize(cols_ * rows_);
    
    // Clear new buffer
    for (auto& cell : buffer_) {
        cell.clear();
    }
    
    // Copy old content
    int copy_cols = std::min(old_cols, cols_);
    int copy_rows = std::min(old_rows, rows_);
    
    for (int row = 0; row < copy_rows; ++row) {
        for (int col = 0; col < copy_cols; ++col) {
            buffer_[row * cols_ + col] = old_buffer[row * old_cols + col];
        }
    }
    
    // Update scroll region
    scroll_top_ = 0;
    scroll_bottom_ = rows_ - 1;
    
    // Adjust cursor
    adjustCursorForResize(old_cols, old_rows);
    
    // Update tab stops
    tab_stops_.resize(cols_, false);
    for (int i = TAB_STOP; i < cols_; i += TAB_STOP) {
        if (i < cols_) tab_stops_[i] = true;
    }
    
    markRangeDamage(0, rows_ - 1);
}

void Screen::setCursor(int col, int row) {
    cursor_col_ = std::max(0, std::min(col, cols_ - 1));
    cursor_row_ = std::max(scroll_top_, std::min(row, scroll_bottom_));
}

void Screen::moveCursor(int dcol, int drow) {
    setCursor(cursor_col_ + dcol, cursor_row_ + drow);
}

void Screen::saveCursor() {
    saved_cursor_col_ = cursor_col_;
    saved_cursor_row_ = cursor_row_;
}

void Screen::restoreCursor() {
    cursor_col_ = saved_cursor_col_;
    cursor_row_ = saved_cursor_row_;
}

void Screen::updateCursorBlink(uint64_t current_time_ms) {
    if (!cursor_blink_ || !cursor_visible_) {
        cursor_blink_state_ = true;  // Always show cursor if blink disabled
        return;
    }

    if (cursor_blink_time_ == 0) {
        cursor_blink_time_ = current_time_ms;
        return;
    }

    uint64_t elapsed = current_time_ms - cursor_blink_time_;
    if (elapsed >= CURSOR_BLINK_INTERVAL_MS) {
        cursor_blink_state_ = !cursor_blink_state_;
        cursor_blink_time_ = current_time_ms;
        // Mark cursor row as damaged to trigger redraw
        markDamage(cursor_row_);
    }
}

void Screen::write(char32_t codepoint) {
    if (codepoint == 0) return;
    
    // Handle wide characters
    bool is_wide = (codepoint >= 0x1100 && codepoint <= 0x115F) ||  // Hangul
                   (codepoint >= 0x2E80 && codepoint <= 0x9FFF) ||  // CJK
                   (codepoint >= 0xAC00 && codepoint <= 0xD7AF) ||  // Hangul Syllables
                   (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||  // CJK Compatibility
                   (codepoint >= 0xFF01 && codepoint <= 0xFF60);    // Fullwidth ASCII
    
    if (insert_mode_) {
        insertCharacters(1);
    }
    
    int row = cursor_row_;
    int col = cursor_col_;
    
    if (row >= 0 && row < rows_ && col >= 0 && col < cols_) {
        Cell& cell = buffer_[row * cols_ + col];
        cell.codepoint = codepoint;
        cell.attrs = current_attrs_;
        cell.attrs.wide = is_wide;
        
        // Mark continuation cell for wide characters
        if (is_wide && col + 1 < cols_) {
            Cell& next = buffer_[row * cols_ + col + 1];
            next.clear();
            next.attrs.wide_continuation = true;
        }
        
        markDamage(row);
    }
    
    // Advance cursor
    cursor_col_++;
    if (is_wide) cursor_col_++;
    
    // Handle line wrapping
    if (cursor_col_ >= cols_) {
        if (auto_wrap_) {
            cursor_col_ = 0;
            lineFeed();
        } else {
            cursor_col_ = cols_ - 1;
        }
    }
}

void Screen::write(const char* utf8, size_t len) {
    size_t i = 0;
    while (i < len) {
        size_t seq_len;
        char32_t cp = utf8_decode(utf8 + i, seq_len);
        write(cp);
        i += seq_len;
    }
}

void Screen::writeString(std::string_view str) {
    write(str.data(), str.size());
}

void Screen::lineFeed() {
    cursor_row_++;
    if (cursor_row_ > scroll_bottom_) {
        cursor_row_ = scroll_bottom_;
        scrollUpInRegion(1);
    }
    markDamage(cursor_row_);
}

void Screen::carriageReturn() {
    cursor_col_ = 0;
}

void Screen::backspace() {
    if (cursor_col_ > 0) {
        cursor_col_--;
    }
}

void Screen::horizontalTab() {
    // Find next tab stop
    for (int col = cursor_col_ + 1; col < cols_; ++col) {
        if (tab_stops_[col]) {
            cursor_col_ = col;
            return;
        }
    }
    cursor_col_ = cols_ - 1;
}

void Screen::reverseLineFeed() {
    cursor_row_--;
    if (cursor_row_ < scroll_top_) {
        cursor_row_ = scroll_top_;
        scrollDownInRegion(1);
    }
    markDamage(cursor_row_);
}

void Screen::index() {
    lineFeed();
}

void Screen::nextLine() {
    carriageReturn();
    lineFeed();
}

void Screen::eraseInDisplay(int mode) {
    switch (mode) {
        case 0:  // Erase from cursor to end of display
            eraseInLine(0);
            for (int row = cursor_row_ + 1; row < rows_; ++row) {
                for (int col = 0; col < cols_; ++col) {
                    buffer_[row * cols_ + col].clear();
                }
            }
            markRangeDamage(cursor_row_, rows_ - 1);
            break;
            
        case 1:  // Erase from start of display to cursor
            for (int row = 0; row < cursor_row_; ++row) {
                for (int col = 0; col < cols_; ++col) {
                    buffer_[row * cols_ + col].clear();
                }
            }
            eraseInLine(1);
            markRangeDamage(0, cursor_row_);
            break;
            
        case 2:  // Erase entire display
        case 3:  // Erase entire display and scrollback
            for (auto& cell : buffer_) {
                cell.clear();
            }
            if (mode == 3) {
                clearHistory();
            }
            damage_.full_screen = true;
            break;
    }
}

void Screen::eraseInLine(int mode) {
    int row = cursor_row_;
    switch (mode) {
        case 0:  // Erase from cursor to end of line
            for (int col = cursor_col_; col < cols_; ++col) {
                buffer_[row * cols_ + col].clear();
            }
            break;
            
        case 1:  // Erase from start of line to cursor
            for (int col = 0; col <= cursor_col_; ++col) {
                buffer_[row * cols_ + col].clear();
            }
            break;
            
        case 2:  // Erase entire line
            for (int col = 0; col < cols_; ++col) {
                buffer_[row * cols_ + col].clear();
            }
            break;
    }
    markDamage(row);
}

void Screen::eraseCharacters(int n) {
    int row = cursor_row_;
    int start = cursor_col_;
    int end = std::min(start + n, cols_);
    
    for (int col = start; col < end; ++col) {
        buffer_[row * cols_ + col].clear();
    }
    markDamage(row);
}

void Screen::deleteCharacters(int n) {
    int row = cursor_row_;
    int start = cursor_col_;
    int end = std::min(start + n, cols_);
    
    // Shift characters left
    for (int col = start; col < cols_ - (end - start); ++col) {
        buffer_[row * cols_ + col] = buffer_[row * cols_ + col + (end - start)];
    }
    
    // Clear vacated cells
    for (int col = cols_ - (end - start); col < cols_; ++col) {
        buffer_[row * cols_ + col].clear();
    }
    markDamage(row);
}

void Screen::insertCharacters(int n) {
    int row = cursor_row_;
    int start = cursor_col_;
    n = std::min(n, cols_ - start);
    
    // Shift characters right
    for (int col = cols_ - 1; col >= start + n; --col) {
        buffer_[row * cols_ + col] = buffer_[row * cols_ + col - n];
    }
    
    // Clear inserted cells
    for (int col = start; col < start + n; ++col) {
        buffer_[row * cols_ + col].clear();
    }
    markDamage(row);
}

void Screen::insertLines(int n) {
    n = std::min(n, scroll_bottom_ - cursor_row_ + 1);
    
    // Shift lines down
    for (int row = scroll_bottom_; row >= cursor_row_ + n; --row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col] = buffer_[(row - n) * cols_ + col];
        }
    }
    
    // Clear inserted lines
    for (int row = cursor_row_; row < cursor_row_ + n; ++row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col].clear();
        }
    }
    markRangeDamage(cursor_row_, scroll_bottom_);
}

void Screen::deleteLines(int n) {
    n = std::min(n, scroll_bottom_ - cursor_row_ + 1);
    
    // Save top lines to history
    for (int i = 0; i < n && cursor_row_ == scroll_top_; ++i) {
        std::vector<Cell> line(cols_);
        for (int col = 0; col < cols_; ++col) {
            line[col] = buffer_[(cursor_row_ + i) * cols_ + col];
        }
        pushToHistory(line, false);
    }
    
    // Shift lines up
    for (int row = cursor_row_; row <= scroll_bottom_ - n; ++row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col] = buffer_[(row + n) * cols_ + col];
        }
    }
    
    // Clear vacated lines
    for (int row = scroll_bottom_ - n + 1; row <= scroll_bottom_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col].clear();
        }
    }
    markRangeDamage(cursor_row_, scroll_bottom_);
}

void Screen::scrollUp(int lines) {
    scrollUpInRegion(lines);
}

void Screen::scrollDown(int lines) {
    scrollDownInRegion(lines);
}

void Screen::scrollUpInRegion(int lines) {
    lines = std::min(lines, scroll_bottom_ - scroll_top_ + 1);
    
    // Save scrolled lines to history
    for (int i = 0; i < lines; ++i) {
        std::vector<Cell> line(cols_);
        for (int col = 0; col < cols_; ++col) {
            line[col] = buffer_[(scroll_top_ + i) * cols_ + col];
        }
        pushToHistory(line, i < lines - 1);
    }
    
    // Shift lines up
    for (int row = scroll_top_; row <= scroll_bottom_ - lines; ++row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col] = buffer_[(row + lines) * cols_ + col];
        }
    }
    
    // Clear vacated lines
    for (int row = scroll_bottom_ - lines + 1; row <= scroll_bottom_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col].clear();
        }
    }
    markRangeDamage(scroll_top_, scroll_bottom_);
}

void Screen::scrollDownInRegion(int lines) {
    lines = std::min(lines, scroll_bottom_ - scroll_top_ + 1);
    
    // Shift lines down
    for (int row = scroll_bottom_; row >= scroll_top_ + lines; --row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col] = buffer_[(row - lines) * cols_ + col];
        }
    }
    
    // Clear inserted lines
    for (int row = scroll_top_; row < scroll_top_ + lines; ++row) {
        for (int col = 0; col < cols_; ++col) {
            buffer_[row * cols_ + col].clear();
        }
    }
    markRangeDamage(scroll_top_, scroll_bottom_);
}

void Screen::setScrollRegion(int top, int bottom) {
    scroll_top_ = std::max(0, std::min(top, rows_ - 1));
    scroll_bottom_ = std::max(scroll_top_, std::min(bottom, rows_ - 1));
}

void Screen::resetScrollRegion() {
    scroll_top_ = 0;
    scroll_bottom_ = rows_ - 1;
}

void Screen::setTabStop() {
    if (cursor_col_ < cols_) {
        tab_stops_[cursor_col_] = true;
    }
}

void Screen::clearTabStop() {
    if (cursor_col_ < cols_) {
        tab_stops_[cursor_col_] = false;
    }
}

void Screen::clearAllTabStops() {
    std::fill(tab_stops_.begin(), tab_stops_.end(), false);
}

void Screen::setForeground(Color color) {
    current_attrs_.fg_color = color & 0xFFFFFF;
}

void Screen::setBackground(Color color) {
    current_attrs_.bg_color = color & 0xFFFFFF;
}

void Screen::resetAttributes() {
    current_attrs_ = {};
    current_attrs_.fg_color = colors::DEFAULT_FG;
    current_attrs_.bg_color = colors::DEFAULT_BG;
}

void Screen::setAttribute(int attr, bool on) {
    switch (attr) {
        case 1: current_attrs_.bold = on; break;
        case 3: current_attrs_.italic = on; break;
        case 4: current_attrs_.underline = on; break;
        case 5: current_attrs_.blink = on; break;
        case 7: current_attrs_.inverse = on; break;
        case 8: current_attrs_.invisible = on; break;
        case 9: current_attrs_.strikethrough = on; break;
        case 22: current_attrs_.bold = false; break;
        case 23: current_attrs_.italic = false; break;
        case 24: current_attrs_.underline = false; break;
        case 25: current_attrs_.blink = false; break;
        case 27: current_attrs_.inverse = false; break;
        case 28: current_attrs_.invisible = false; break;
        case 29: current_attrs_.strikethrough = false; break;
    }
}

const Cell* Screen::getRow(int row) const {
    if (row < 0 || row >= rows_) return nullptr;
    return &buffer_[row * cols_];
}

Cell* Screen::getRow(int row) {
    if (row < 0 || row >= rows_) return nullptr;
    return &buffer_[row * cols_];
}

const Cell& Screen::getCell(int col, int row) const {
    static Cell empty;
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return empty;
    return buffer_[row * cols_ + col];
}

Cell& Screen::getCell(int col, int row) {
    static Cell empty;
    if (col < 0 || col >= cols_ || row < 0 || row >= rows_) return empty;
    return buffer_[row * cols_ + col];
}

void Screen::clearHistory() {
    history_.clear();
}

void Screen::setSelection(int start_col, int start_row, int end_col, int end_row) {
    selection_.start_col = start_col;
    selection_.start_row = start_row;
    selection_.end_col = end_col;
    selection_.end_row = end_row;
    selection_.active = true;
    damage_.full_screen = true;
}

std::string Screen::getSelectedText() const {
    if (!selection_.active) return "";
    
    std::string result;
    int start_r = std::min(selection_.start_row, selection_.end_row);
    int end_r = std::max(selection_.start_row, selection_.end_row);
    
    for (int row = start_r; row <= end_r; ++row) {
        const Cell* line = getRow(row);
        if (!line) continue;
        
        int start_c, end_c;
        if (selection_.rectangular) {
            start_c = std::min(selection_.start_col, selection_.end_col);
            end_c = std::max(selection_.start_col, selection_.end_col);
        } else {
            if (row == start_r) {
                start_c = (selection_.start_row < selection_.end_row) ? selection_.start_col : selection_.end_col;
            } else {
                start_c = 0;
            }
            if (row == end_r) {
                end_c = (selection_.start_row < selection_.end_row) ? selection_.end_col : selection_.start_col;
            } else {
                end_c = cols_ - 1;
            }
        }
        
        for (int col = start_c; col <= end_c; ++col) {
            if (!line[col].empty() && !line[col].attrs.wide_continuation) {
                // Convert codepoint to UTF-8
                char32_t cp = line[col].codepoint;
                if (cp < 0x80) {
                    result += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    result += static_cast<char>(0xC0 | (cp >> 6));
                    result += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    result += static_cast<char>(0xE0 | (cp >> 12));
                    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    result += static_cast<char>(0xF0 | (cp >> 18));
                    result += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    result += static_cast<char>(0x80 | (cp & 0x3F));
                }
            }
        }
        
        if (row < end_r) {
            result += '\n';
        }
    }
    
    return result;
}

void Screen::pushToHistory(const std::vector<Cell>& line, bool wrapped) {
    if (history_.size() >= MAX_HISTORY) {
        history_.pop_front();
    }
    history_.push_back({line, wrapped});
}

void Screen::adjustCursorForResize(int old_cols, int old_rows) {
    cursor_col_ = std::min(cursor_col_, cols_ - 1);
    cursor_row_ = std::min(cursor_row_, rows_ - 1);
}

int Screen::effectiveRow(int row) const {
    if (origin_mode_) {
        return scroll_top_ + row;
    }
    return row;
}

void Screen::markDamage(int row) {
    damage_.addRow(row);
}

void Screen::markRangeDamage(int start_row, int end_row) {
    damage_.addRange(start_row, end_row);
}

void Screen::reset() {
    for (auto& cell : buffer_) {
        cell.clear();
    }
    
    cursor_col_ = 0;
    cursor_row_ = 0;
    saved_cursor_col_ = 0;
    saved_cursor_row_ = 0;
    cursor_visible_ = true;
    cursor_style_ = 0;
    
    resetAttributes();
    
    scroll_top_ = 0;
    scroll_bottom_ = rows_ - 1;
    
    insert_mode_ = false;
    auto_wrap_ = true;
    origin_mode_ = false;
    application_keypad_ = false;
    application_cursor_ = false;
    bracketed_paste_ = false;
    mouse_mode_ = 0;
    
    selection_.clear();
    clearHistory();
    
    damage_.full_screen = true;
}

void Screen::dump() const {
    for (int row = 0; row < rows_; ++row) {
        for (int col = 0; col < cols_; ++col) {
            const Cell& cell = buffer_[row * cols_ + col];
            if (cell.empty()) {
                std::cout << ' ';
            } else if (cell.codepoint < 128) {
                std::cout << static_cast<char>(cell.codepoint);
            } else {
                std::cout << '?';
            }
        }
        std::cout << '|' << std::endl;
    }
}

} // namespace tx
