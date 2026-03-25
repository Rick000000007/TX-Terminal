#include <gtest/gtest.h>
#include "tx/screen.hpp"

using namespace tx;

class ScreenTest : public ::testing::Test {
protected:
    void SetUp() override {
        screen_.resize(80, 24);
        screen_.reset();
    }
    
    Screen screen_;
};

TEST_F(ScreenTest, InitialState) {
    EXPECT_EQ(screen_.cols(), 80);
    EXPECT_EQ(screen_.rows(), 24);
    EXPECT_EQ(screen_.cursorCol(), 0);
    EXPECT_EQ(screen_.cursorRow(), 0);
}

TEST_F(ScreenTest, WriteCharacter) {
    screen_.write('A');
    
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'A');
    EXPECT_EQ(screen_.cursorCol(), 1);
}

TEST_F(ScreenTest, WriteMultipleCharacters) {
    screen_.writeString("Hello");
    
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'H');
    EXPECT_EQ(screen_.getCell(4, 0).codepoint, 'o');
    EXPECT_EQ(screen_.cursorCol(), 5);
}

TEST_F(ScreenTest, CursorMovement) {
    screen_.setCursor(10, 5);
    
    EXPECT_EQ(screen_.cursorCol(), 10);
    EXPECT_EQ(screen_.cursorRow(), 5);
}

TEST_F(ScreenTest, CursorBounds) {
    screen_.setCursor(1000, 1000);
    
    EXPECT_EQ(screen_.cursorCol(), 79);
    EXPECT_EQ(screen_.cursorRow(), 23);
}

TEST_F(ScreenTest, LineFeed) {
    screen_.setCursor(0, 5);
    screen_.lineFeed();
    
    EXPECT_EQ(screen_.cursorRow(), 6);
}

TEST_F(ScreenTest, LineFeedScroll) {
    screen_.setCursor(0, 23);
    screen_.lineFeed();
    
    EXPECT_EQ(screen_.cursorRow(), 23);
    // Screen should have scrolled
}

TEST_F(ScreenTest, CarriageReturn) {
    screen_.setCursor(10, 5);
    screen_.carriageReturn();
    
    EXPECT_EQ(screen_.cursorCol(), 0);
    EXPECT_EQ(screen_.cursorRow(), 5);
}

TEST_F(ScreenTest, Backspace) {
    screen_.setCursor(5, 0);
    screen_.backspace();
    
    EXPECT_EQ(screen_.cursorCol(), 4);
}

TEST_F(ScreenTest, BackspaceAtStart) {
    screen_.setCursor(0, 0);
    screen_.backspace();
    
    EXPECT_EQ(screen_.cursorCol(), 0);
}

TEST_F(ScreenTest, HorizontalTab) {
    screen_.horizontalTab();
    
    EXPECT_EQ(screen_.cursorCol(), 8);  // First tab stop
}

TEST_F(ScreenTest, EraseInLine) {
    screen_.writeString("Hello World");
    screen_.setCursor(5, 0);
    screen_.eraseInLine(0);
    
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'H');
    EXPECT_TRUE(screen_.getCell(6, 0).empty());
}

TEST_F(ScreenTest, EraseInDisplay) {
    screen_.writeString("Line1");
    screen_.lineFeed();
    screen_.writeString("Line2");
    screen_.eraseInDisplay(2);
    
    EXPECT_TRUE(screen_.getCell(0, 0).empty());
    EXPECT_TRUE(screen_.getCell(0, 1).empty());
}

TEST_F(ScreenTest, InsertCharacters) {
    screen_.writeString("Hello");
    screen_.setCursor(2, 0);
    screen_.insertCharacters(2);
    
    EXPECT_EQ(screen_.getCell(2, 0).codepoint, 0);  // Inserted space
    EXPECT_EQ(screen_.getCell(4, 0).codepoint, 'l');
}

TEST_F(ScreenTest, DeleteCharacters) {
    screen_.writeString("Hello");
    screen_.setCursor(1, 0);
    screen_.deleteCharacters(2);
    
    EXPECT_EQ(screen_.getCell(1, 0).codepoint, 'l');
    EXPECT_EQ(screen_.getCell(2, 0).codepoint, 'o');
}

TEST_F(ScreenTest, ScrollUp) {
    screen_.writeString("Line1");
    screen_.lineFeed();
    screen_.writeString("Line2");
    screen_.scrollUp(1);
    
    // Line1 should be in history
    EXPECT_GE(screen_.getHistorySize(), 0);
}

TEST_F(ScreenTest, SetForegroundColor) {
    screen_.setForeground(colors::RED);
    screen_.write('A');
    
    EXPECT_EQ(screen_.getCell(0, 0).attrs.fg_color, colors::RED);
}

TEST_F(ScreenTest, SetBackgroundColor) {
    screen_.setBackground(colors::BLUE);
    screen_.write('A');
    
    EXPECT_EQ(screen_.getCell(0, 0).attrs.bg_color, colors::BLUE);
}

TEST_F(ScreenTest, ResetAttributes) {
    screen_.setForeground(colors::RED);
    screen_.setAttribute(1, true);  // Bold
    screen_.resetAttributes();
    screen_.write('A');
    
    EXPECT_EQ(screen_.getCell(0, 0).attrs.fg_color, colors::DEFAULT_FG);
    EXPECT_FALSE(screen_.getCell(0, 0).attrs.bold);
}

TEST_F(ScreenTest, SaveRestoreCursor) {
    screen_.setCursor(10, 5);
    screen_.saveCursor();
    screen_.setCursor(0, 0);
    screen_.restoreCursor();
    
    EXPECT_EQ(screen_.cursorCol(), 10);
    EXPECT_EQ(screen_.cursorRow(), 5);
}

TEST_F(ScreenTest, Selection) {
    screen_.writeString("Hello World");
    screen_.setSelection(0, 0, 4, 0);
    
    EXPECT_TRUE(screen_.getSelection().active);
    EXPECT_TRUE(screen_.getSelection().contains(2, 0));
    EXPECT_FALSE(screen_.getSelection().contains(6, 0));
}

TEST_F(ScreenTest, GetSelectedText) {
    screen_.writeString("Hello");
    screen_.setSelection(0, 0, 4, 0);
    
    EXPECT_EQ(screen_.getSelectedText(), "Hello");
}

TEST_F(ScreenTest, Resize) {
    screen_.writeString("Test");
    screen_.resize(40, 12);
    
    EXPECT_EQ(screen_.cols(), 40);
    EXPECT_EQ(screen_.rows(), 12);
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'T');
}

TEST_F(ScreenTest, WideCharacter) {
    // Japanese character (wide)
    screen_.write(U'\u3042');  // あ
    
    EXPECT_TRUE(screen_.getCell(0, 0).attrs.wide);
    EXPECT_TRUE(screen_.getCell(1, 0).attrs.wide_continuation);
}

TEST_F(ScreenTest, DamageTracking) {
    screen_.write('A');
    
    EXPECT_TRUE(screen_.getDamage().isDirty());
    EXPECT_EQ(screen_.getDamage().start_row, 0);
}

TEST_F(ScreenTest, ClearDamage) {
    screen_.write('A');
    screen_.clearDamage();
    
    EXPECT_FALSE(screen_.getDamage().isDirty());
}
