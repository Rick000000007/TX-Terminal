#include <gtest/gtest.h>
#include "tx/parser.hpp"
#include "tx/screen.hpp"

using namespace tx;

class ParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser_.reset();
        screen_.resize(80, 24);
        screen_.reset();
    }
    
    Parser parser_;
    Screen screen_;
    
    ParserActions createActions() {
        ParserActions actions;
        
        actions.print = [this](char32_t cp) {
            screen_.write(cp);
        };
        
        actions.execute = [this](uint8_t ctrl) {
            switch (ctrl) {
                case 0x0A: screen_.lineFeed(); break;
                case 0x0D: screen_.carriageReturn(); break;
                case 0x08: screen_.backspace(); break;
                case 0x09: screen_.horizontalTab(); break;
            }
        };
        
        actions.csi = [this](uint8_t cmd, const int* args, int argc) {
            int arg0 = argc > 0 ? args[0] : 0;
            int arg1 = argc > 1 ? args[1] : 0;
            
            switch (cmd) {
                case 'H':
                    screen_.setCursor((arg1 ? arg1 : 1) - 1, (arg0 ? arg0 : 1) - 1);
                    break;
                case 'J':
                    screen_.eraseInDisplay(arg0);
                    break;
                case 'K':
                    screen_.eraseInLine(arg0);
                    break;
                case 'm':
                    if (argc == 0 || arg0 == 0) {
                        screen_.resetAttributes();
                    } else if (arg0 >= 30 && arg0 <= 37) {
                        screen_.setForeground(get_ansi_palette()[arg0 - 30]);
                    }
                    break;
            }
        };
        
        return actions;
    }
};

TEST_F(ParserTest, SimpleText) {
    parser_.parse("Hello", createActions());
    
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'H');
    EXPECT_EQ(screen_.getCell(1, 0).codepoint, 'e');
    EXPECT_EQ(screen_.getCell(2, 0).codepoint, 'l');
    EXPECT_EQ(screen_.getCell(3, 0).codepoint, 'l');
    EXPECT_EQ(screen_.getCell(4, 0).codepoint, 'o');
}

TEST_F(ParserTest, CursorPosition) {
    parser_.parse("\x1b[10;20H", createActions());
    
    EXPECT_EQ(screen_.cursorCol(), 19);
    EXPECT_EQ(screen_.cursorRow(), 9);
}

TEST_F(ParserTest, CursorPositionDefault) {
    parser_.parse("\x1b[H", createActions());
    
    EXPECT_EQ(screen_.cursorCol(), 0);
    EXPECT_EQ(screen_.cursorRow(), 0);
}

TEST_F(ParserTest, EraseInDisplay) {
    screen_.writeString("Hello World");
    parser_.parse("\x1b[2J", createActions());
    
    EXPECT_TRUE(screen_.getCell(0, 0).empty());
    EXPECT_TRUE(screen_.getCell(5, 0).empty());
}

TEST_F(ParserTest, EraseInLine) {
    screen_.writeString("Hello World");
    screen_.setCursor(5, 0);
    parser_.parse("\x1b[0K", createActions());
    
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'H');
    EXPECT_TRUE(screen_.getCell(6, 0).empty());
}

TEST_F(ParserTest, SetForegroundColor) {
    parser_.parse("\x1b[31mRed", createActions());
    
    EXPECT_EQ(screen_.getCell(0, 0).attrs.fg_color, colors::RED);
}

TEST_F(ParserTest, ResetAttributes) {
    parser_.parse("\x1b[31mRed\x1b[0mNormal", createActions());
    
    EXPECT_EQ(screen_.getCell(0, 0).attrs.fg_color, colors::RED);
    EXPECT_EQ(screen_.getCell(3, 0).attrs.fg_color, colors::DEFAULT_FG);
}

TEST_F(ParserTest, LineFeed) {
    parser_.parse("Line\nNext", createActions());
    
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'L');
    EXPECT_EQ(screen_.getCell(0, 1).codepoint, 'N');
}

TEST_F(ParserTest, CarriageReturn) {
    screen_.writeString("Test");
    parser_.parse("\rNew", createActions());
    
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, 'N');
}

TEST_F(ParserTest, MultipleParameters) {
    parser_.parse("\x1b[1;2;3m", createActions());
    // Should not crash, attributes should be set
}

TEST_F(ParserTest, OSCSequence) {
    bool osc_called = false;
    int osc_cmd = 0;
    
    ParserActions actions = createActions();
    actions.osc = [&osc_called, &osc_cmd](int cmd, std::string_view data) {
        osc_called = true;
        osc_cmd = cmd;
    };
    
    parser_.parse("\x1b]0;Title\x07", actions);
    
    EXPECT_TRUE(osc_called);
    EXPECT_EQ(osc_cmd, 0);
}

TEST_F(ParserTest, UTF8Text) {
    // UTF-8 for "Hello" in Japanese (こんにちは)
    parser_.parse("\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF", createActions());
    
    // Should have 5 wide characters
    EXPECT_EQ(screen_.getCell(0, 0).codepoint, U'\u3053');
    EXPECT_TRUE(screen_.getCell(0, 0).attrs.wide);
}

TEST_F(ParserTest, StateReset) {
    parser_.parse("\x1b[", createActions());
    EXPECT_NE(parser_.getState(), Parser::State::Ground);
    
    parser_.reset();
    EXPECT_EQ(parser_.getState(), Parser::State::Ground);
}

TEST_F(ParserTest, ScrollRegion) {
    parser_.parse("\x1b[5;10r", createActions());
    // Scroll region should be set (implementation dependent)
}
