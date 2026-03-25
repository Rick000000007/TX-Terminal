#pragma once

#include "common.hpp"
#include <string>
#include <functional>
#include <atomic>

#if defined(TX_ANDROID)
    // Android uses a different approach - pseudo-terminal via /dev/ptmx
    #include <termios.h>
#else
    #include <termios.h>
#endif

namespace tx {

// Result type for PTY operations
struct PTYResult {
    bool success;
    std::string error;
    
    static PTYResult ok() { return {true, ""}; }
    static PTYResult fail(const std::string& msg) { return {false, msg}; }
};

class PTY {
public:
    using DataCallback = std::function<void(const uint8_t* data, size_t len)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    using ExitCallback = std::function<void(int exit_code)>;
    
    PTY();
    ~PTY();
    
    // Non-copyable
    PTY(const PTY&) = delete;
    PTY& operator=(const PTY&) = delete;
    
    // Movable
    PTY(PTY&& other) noexcept;
    PTY& operator=(PTY&& other) noexcept;
    
    // Open PTY and spawn shell
    PTYResult open(const std::string& shell = "/system/bin/sh",
                   const std::vector<std::string>& env = {});
    
    // Close PTY and terminate child
    void close();
    
    // Check if PTY is open
    bool isOpen() const { return master_fd_ >= 0; }
    
    // Write data to PTY
    PTYResult write(const uint8_t* data, size_t len);
    PTYResult write(const std::string& data);
    
    // Read available data (non-blocking)
    PTYResult read();
    
    // Get file descriptor for polling
    int getFD() const { return master_fd_; }
    
    // Resize terminal
    void resize(int cols, int rows);
    
    // Get current size
    int getCols() const { return cols_; }
    int getRows() const { return rows_; }
    
    // Set callbacks
    void setDataCallback(DataCallback cb) { data_cb_ = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { error_cb_ = std::move(cb); }
    void setExitCallback(ExitCallback cb) { exit_cb_ = std::move(cb); }
    
    // Check if child is still running
    bool isChildRunning() const;
    
    // Send signal to child
    void sendSignal(int sig);
    
private:
    int master_fd_ = -1;
    pid_t child_pid_ = -1;
    int cols_ = 80;
    int rows_ = 24;
    
    DataCallback data_cb_;
    ErrorCallback error_cb_;
    ExitCallback exit_cb_;
    
    std::vector<uint8_t> read_buffer_;
    
    // Platform-specific helpers
    bool setupMasterPTY();
    bool configureTerminal();
    pid_t spawnChild(const std::string& shell, const std::vector<std::string>& env);
    
    // Android-specific
    #if defined(TX_ANDROID)
    int openPseudoTerminal();
    #endif
};

} // namespace tx
