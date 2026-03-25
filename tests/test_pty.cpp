#include <gtest/gtest.h>
#include "tx/pty.hpp"
#include <thread>
#include <chrono>

using namespace tx;

class PTYTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Tests may need root or special permissions on some systems
    }
    
    void TearDown() override {
        pty_.close();
    }
    
    PTY pty_;
};

TEST_F(PTYTest, OpenClose) {
    auto result = pty_.open("/bin/sh");
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(pty_.isOpen());
    
    pty_.close();
    EXPECT_FALSE(pty_.isOpen());
}

TEST_F(PTYTest, WriteRead) {
    auto result = pty_.open("/bin/sh");
    ASSERT_TRUE(result.success);
    
    // Set up data callback
    std::string received;
    pty_.setDataCallback([&received](const uint8_t* data, size_t len) {
        received.append(reinterpret_cast<const char*>(data), len);
    });
    
    // Write a command
    pty_.write("echo test123\n");
    
    // Wait for response
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    pty_.read();
    
    // Should have received output
    EXPECT_FALSE(received.empty());
}

TEST_F(PTYTest, Resize) {
    auto result = pty_.open("/bin/sh");
    ASSERT_TRUE(result.success);
    
    pty_.resize(100, 50);
    
    EXPECT_EQ(pty_.getCols(), 100);
    EXPECT_EQ(pty_.getRows(), 50);
}

TEST_F(PTYTest, IsChildRunning) {
    auto result = pty_.open("/bin/sh");
    ASSERT_TRUE(result.success);
    
    EXPECT_TRUE(pty_.isChildRunning());
    
    // Send exit command
    pty_.write("exit\n");
    
    // Wait for child to exit
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_FALSE(pty_.isChildRunning());
}

TEST_F(PTYTest, MultipleInstances) {
    PTY pty1, pty2;
    
    auto result1 = pty1.open("/bin/sh");
    auto result2 = pty2.open("/bin/sh");
    
    EXPECT_TRUE(result1.success);
    EXPECT_TRUE(result2.success);
    
    EXPECT_NE(pty1.getFD(), pty2.getFD());
}
