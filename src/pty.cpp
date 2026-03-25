#include "tx/pty.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <cerrno>
#include <cstring>
#include <iostream>

#if defined(TX_ANDROID)
    #include <android/log.h>
    #define LOG_TAG "TX_PTY"
    #define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
    #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
    #define LOGD(...) do { } while(0)
    #define LOGE(...) std::cerr
#endif

namespace tx {

PTY::PTY() = default;

PTY::~PTY() {
    this->close();
}

PTY::PTY(PTY&& other) noexcept
    : master_fd_(other.master_fd_)
    , child_pid_(other.child_pid_)
    , cols_(other.cols_)
    , rows_(other.rows_)
    , data_cb_(std::move(other.data_cb_))
    , error_cb_(std::move(other.error_cb_))
    , exit_cb_(std::move(other.exit_cb_))
    , read_buffer_(std::move(other.read_buffer_)) {
    other.master_fd_ = -1;
    other.child_pid_ = -1;
}

PTY& PTY::operator=(PTY&& other) noexcept {
    if (this != &other) {
    this->close();
        master_fd_ = other.master_fd_;
        child_pid_ = other.child_pid_;
        cols_ = other.cols_;
        rows_ = other.rows_;
        data_cb_ = std::move(other.data_cb_);
        error_cb_ = std::move(other.error_cb_);
        exit_cb_ = std::move(other.exit_cb_);
        read_buffer_ = std::move(other.read_buffer_);
        other.master_fd_ = -1;
        other.child_pid_ = -1;
    }
    return *this;
}

PTYResult PTY::open(const std::string& shell, const std::vector<std::string>& env) {
    if (master_fd_ >= 0) {
        return PTYResult::fail("PTY already open");
    }
    
    // Open PTY master
    master_fd_ = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC | O_NONBLOCK);
    if (master_fd_ < 0) {
        return PTYResult::fail(std::string("posix_openpt failed: ") + strerror(errno));
    }
    
    // Grant and unlock PTY
    if (grantpt(master_fd_) < 0) {
    this->close();
        return PTYResult::fail(std::string("grantpt failed: ") + strerror(errno));
    }
    
    if (unlockpt(master_fd_) < 0) {
    this->close();
        return PTYResult::fail(std::string("unlockpt failed: ") + strerror(errno));
    }
    
    // Get slave name
    char* slave_name = ptsname(master_fd_);
    if (!slave_name) {
    this->close();
        return PTYResult::fail(std::string("ptsname failed: ") + strerror(errno));
    }
    
    LOGD("PTY opened: master=%d, slave=%s", master_fd_, slave_name);
    
    // Fork child process
    child_pid_ = fork();
    if (child_pid_ < 0) {
    this->close();
        return PTYResult::fail(std::string("fork failed: ") + strerror(errno));
    }
    
    if (child_pid_ == 0) {
        // Child process
        
        // Create new session
        if (setsid() < 0) {
            _exit(1);
        }
        
        // Open slave PTY
        int slave_fd = ::open(slave_name, O_RDWR | O_NOCTTY);
        if (slave_fd < 0) {
            _exit(1);
        }
        
        // Set terminal attributes
        struct termios tios;
        if (tcgetattr(slave_fd, &tios) == 0) {
            // Configure for typical terminal behavior
            cfmakeraw(&tios);
            tios.c_cc[VMIN] = 1;
            tios.c_cc[VTIME] = 0;
            tcsetattr(slave_fd, TCSANOW, &tios);
        }
        
        // Duplicate to stdio
        dup2(slave_fd, STDIN_FILENO);
        dup2(slave_fd, STDOUT_FILENO);
        dup2(slave_fd, STDERR_FILENO);
        
        // Close unnecessary fds
        if (slave_fd > STDERR_FILENO) {
            ::close(slave_fd);
        }
        ::close(master_fd_);
        
        // Clear close-on-exec for stdio
        fcntl(STDIN_FILENO, F_SETFD, 0);
        fcntl(STDOUT_FILENO, F_SETFD, 0);
        fcntl(STDERR_FILENO, F_SETFD, 0);
        
        // Set environment variables
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        
        // Set terminal size
        struct winsize ws;
        ws.ws_col = cols_;
        ws.ws_row = rows_;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws);
        
        // Apply custom environment
        for (const auto& var : env) {
            size_t pos = var.find('=');
            if (pos != std::string::npos) {
                setenv(var.substr(0, pos).c_str(), var.substr(pos + 1).c_str(), 1);
            }
        }
        
        // Execute shell
        execl(shell.c_str(), shell.c_str(), nullptr);
        
        // If exec fails, try /system/bin/sh (Android fallback)
        execl("/system/bin/sh", "sh", nullptr);
        
        // Last resort
        _exit(127);
    }
    
    // Parent process
    read_buffer_.resize(4096);
    
    LOGD("Child spawned: pid=%d", child_pid_);
    
    return PTYResult::ok();
}

void PTY::close() {
    if (master_fd_ >= 0) {
        ::close(master_fd_);
        master_fd_ = -1;
    }
    
    if (child_pid_ > 0) {
        // Send SIGTERM first
        kill(child_pid_, SIGTERM);
        
        // Wait briefly for graceful termination
        int status;
        pid_t result = waitpid(child_pid_, &status, WNOHANG);
        
        if (result == 0) {
            // Still running, wait a bit more
            usleep(100000);  // 100ms
            result = waitpid(child_pid_, &status, WNOHANG);
            
            if (result == 0) {
                // Force kill
                kill(child_pid_, SIGKILL);
                waitpid(child_pid_, &status, 0);
            }
        }
        
        if (exit_cb_ && WIFEXITED(status)) {
            exit_cb_(WEXITSTATUS(status));
        }
        
        child_pid_ = -1;
    }
}

PTYResult PTY::write(const uint8_t* data, size_t len) {
    if (master_fd_ < 0) {
        return PTYResult::fail("PTY not open");
    }
    
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(master_fd_, data + written, len - written);
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            return PTYResult::fail(std::string("write failed: ") + strerror(errno));
        }
        written += n;
    }
    
    return PTYResult::ok();
}

PTYResult PTY::write(const std::string& data) {
    return write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

PTYResult PTY::read() {
    if (master_fd_ < 0) {
        return PTYResult::fail("PTY not open");
    }
    
    if (!data_cb_) {
        return PTYResult::ok();  // No callback, skip reading
    }
    
    ssize_t total_read = 0;
    
    while (true) {
        ssize_t n = ::read(master_fd_, read_buffer_.data(), read_buffer_.size());
        
        if (n > 0) {
            data_cb_(read_buffer_.data(), n);
            total_read += n;
            
            // If we filled the buffer, there might be more data
            if (static_cast<size_t>(n) == read_buffer_.size()) {
                continue;
            }
        } else if (n == 0) {
            // EOF - child exited
            if (exit_cb_) {
                exit_cb_(0);
            }
            return PTYResult::fail("EOF");
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data available
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                return PTYResult::fail(std::string("read failed: ") + strerror(errno));
            }
        }
    }
    
    return PTYResult::ok();
}

void PTY::resize(int cols, int rows) {
    cols_ = cols;
    rows_ = rows;
    
    if (master_fd_ >= 0) {
        struct winsize ws;
        ws.ws_col = cols;
        ws.ws_row = rows;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
        ioctl(master_fd_, TIOCSWINSZ, &ws);
    }
}

bool PTY::isChildRunning() const {
    if (child_pid_ <= 0) {
        return false;
    }
    
    int status;
    pid_t result = waitpid(child_pid_, &status, WNOHANG);
    
    if (result == 0) {
        return true;  // Still running
    } else if (result == child_pid_) {
        return false;  // Exited
    }
    
    return false;
}

void PTY::sendSignal(int sig) {
    if (child_pid_ > 0) {
        kill(child_pid_, sig);
    }
}

} // namespace tx
