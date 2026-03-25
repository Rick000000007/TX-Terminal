#include "tx/terminal.hpp"
#include "tx/config.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <csignal>

using namespace tx;

static Terminal* g_terminal = nullptr;

void signalHandler(int sig) {
    if (g_terminal) {
        g_terminal->setRunning(false);
    }
}

void errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* terminal = static_cast<Terminal*>(glfwGetWindowUserPointer(window));
    if (terminal) {
        terminal->onKey(key, mods, action == GLFW_PRESS || action == GLFW_REPEAT);
    }
}

void charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* terminal = static_cast<Terminal*>(glfwGetWindowUserPointer(window));
    if (terminal) {
        terminal->onChar(codepoint);
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* terminal = static_cast<Terminal*>(glfwGetWindowUserPointer(window));
    if (terminal) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);
        terminal->onMouseButton(button, action == GLFW_PRESS, mods);
        
        // Handle selection
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            int col = static_cast<int>(x / terminal->getRenderer().getConfig().cell_width);
            int row = static_cast<int>(y / terminal->getRenderer().getConfig().cell_height);
            
            if (action == GLFW_PRESS) {
                terminal->startSelection(col, row);
            } else {
                terminal->endSelection();
            }
        }
    }
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
    auto* terminal = static_cast<Terminal*>(glfwGetWindowUserPointer(window));
    if (terminal) {
        terminal->onMouseMove(static_cast<int>(x), static_cast<int>(y));
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    auto* terminal = static_cast<Terminal*>(glfwGetWindowUserPointer(window));
    if (terminal) {
        terminal->onMouseScroll(static_cast<int>(yoffset));
    }
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* terminal = static_cast<Terminal*>(glfwGetWindowUserPointer(window));
    if (terminal) {
        terminal->onResize(width, height);
    }
}

void dropCallback(GLFWwindow* window, int count, const char** paths) {
    auto* terminal = static_cast<Terminal*>(glfwGetWindowUserPointer(window));
    if (terminal && count > 0) {
        // Paste file paths
        std::string text;
        for (int i = 0; i < count; ++i) {
            if (i > 0) text += " ";
            text += paths[i];
        }
        terminal->pasteText(text);
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Initialize GLFW
    glfwSetErrorCallback(errorCallback);
    
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return 1;
    }
    
    // Set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // Create window
    int window_width = 800;
    int window_height = 600;
    
    GLFWwindow* window = glfwCreateWindow(window_width, window_height, 
                                          "TX Terminal", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return 1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync
    
    // Set up callbacks
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetDropCallback(window, dropCallback);
    
    // Load configuration
    Config::reset();
    Config::loadFromFile(Config::getDefaultPath());
    Config::loadFromFile(Config::getSystemPath());
    
    // Apply command line arguments
    TerminalConfig config;
    Config::applyTo(config);
    
    // Parse command line
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            std::cout << "TX Terminal - A fast terminal emulator for aarch64\n\n";
            std::cout << "Usage: tx [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  -h, --help          Show this help message\n";
            std::cout << "  -v, --version       Show version information\n";
            std::cout << "  -e, --exec <cmd>    Execute command instead of shell\n";
            std::cout << "  --cols <n>          Set number of columns\n";
            std::cout << "  --rows <n>          Set number of rows\n";
            std::cout << "  --font <path>       Set font file path\n";
            std::cout << "  --font-size <n>     Set font size\n";
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "TX Terminal " << VERSION << std::endl;
            return 0;
        } else if (arg == "--exec" || arg == "-e") {
            if (i + 1 < argc) {
                config.shell = argv[++i];
            }
        } else if (arg == "--cols" && i + 1 < argc) {
            config.cols = std::stoi(argv[++i]);
        } else if (arg == "--rows" && i + 1 < argc) {
            config.rows = std::stoi(argv[++i]);
        } else if (arg == "--font" && i + 1 < argc) {
            config.render.font_path = argv[++i];
        } else if (arg == "--font-size" && i + 1 < argc) {
            config.render.font_size = std::stof(argv[++i]);
        }
    }
    
    // Create and initialize terminal
    Terminal terminal;
    g_terminal = &terminal;
    
    if (!terminal.initialize(config)) {
        std::cerr << "Failed to initialize terminal" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    
    // Set window user pointer for callbacks
    glfwSetWindowUserPointer(window, &terminal);
    
    // Handle initial resize
    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    terminal.onResize(fb_width, fb_height);
    
    std::cout << "TX Terminal " << VERSION << " started" << std::endl;
    std::cout << "Press Ctrl+C or close window to exit" << std::endl;
    
    // Main loop
    while (terminal.isRunning() && !glfwWindowShouldClose(window)) {
        // Poll events
        glfwPollEvents();
        
        // Update terminal (process PTY I/O)
        terminal.update();
        
        // Render
        terminal.render();
        
        // Swap buffers
        glfwSwapBuffers(window);
    }
    
    std::cout << "TX Terminal shutting down..." << std::endl;
    
    // Cleanup
    terminal.shutdown();
    g_terminal = nullptr;
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
