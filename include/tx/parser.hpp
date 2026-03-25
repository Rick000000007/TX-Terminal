#pragma once

#include "common.hpp"
#include <string_view>

namespace tx {

// Parser actions - called when sequences are recognized
struct ParserActions {
    // Print a character to the screen
    std::function<void(char32_t codepoint)> print;
    
    // Execute C0 control character (LF, CR, etc.)
    std::function<void(uint8_t ctrl)> execute;
    
    // CSI sequence: cmd is final byte, args are parameters
    std::function<void(uint8_t cmd, const int* args, int argc)> csi;
    
    // OSC sequence: string data
    std::function<void(int cmd, std::string_view data)> osc;
    
    // ESC sequence: intermediate + final
    std::function<void(uint8_t intermediate, uint8_t final_byte)> esc;
    
    // DCS (Device Control String)
    std::function<void(int cmd, const int* args, int argc)> dcs_hook;
    std::function<void(const uint8_t* data, size_t len)> dcs_put;
    std::function<void()> dcs_unhook;
    
    // APC/PM/SOS (rarely used)
    std::function<void(uint8_t type, std::string_view data)> apc_pm_sos;
};

// ANSI/VT100 escape sequence parser
// Implements state machine based on ECMA-48 / VT500 specification
class Parser {
public:
    enum class State {
        Ground,                 // Normal text processing
        Escape,                 // After ESC
        EscapeIntermediate,     // ESC with intermediates
        CSI_Entry,              // After ESC [
        CSI_Param,              // Collecting parameters
        CSI_Intermediate,       // CSI with intermediates
        CSI_Ignore,             // CSI error recovery
        DCS_Entry,              // Device Control String entry
        DCS_Param,              // DCS parameters
        DCS_Intermediate,       // DCS intermediates
        DCS_Passthrough,        // DCS data
        DCS_Ignore,             // DCS error recovery
        OSC_String,             // Operating System Command
        SOS_PM_APC_String,      // SOS/PM/APC strings
    };
    
    Parser();
    ~Parser() = default;
    
    // Parse input data
    void parse(const uint8_t* data, size_t len, const ParserActions& actions);
    void parse(std::string_view data, const ParserActions& actions);
    
    // Reset parser to ground state
    void reset();
    
    // Get current state (for debugging)
    State getState() const { return state_; }
    
private:
    State state_ = State::Ground;
    
    // Parameter collection
    static constexpr int MAX_PARAMS = 16;
    int params_[MAX_PARAMS];
    int param_count_ = 0;
    bool param_has_value_ = false;
    
    // Intermediate bytes
    uint8_t intermediate_ = 0;
    
    // OSC/DCS data collection
    std::string osc_buffer_;
    int osc_cmd_ = 0;
    
    // DCS state
    int dcs_cmd_ = 0;
    
    // State handlers
    void handleGround(uint8_t ch, const ParserActions& actions);
    void handleEscape(uint8_t ch, const ParserActions& actions);
    void handleCSIEntry(uint8_t ch, const ParserActions& actions);
    void handleCSIParam(uint8_t ch, const ParserActions& actions);
    void handleOSC(uint8_t ch, const ParserActions& actions);
    void handleDCSPassthrough(uint8_t ch, const ParserActions& actions);
    
    // Helpers
    void clearParams();
    void paramPush(int value);
    void dispatchCSI(uint8_t cmd, const ParserActions& actions);
    void dispatchOSC(const ParserActions& actions);
    
    // Character classification
    static bool isC0Control(uint8_t ch) { return ch < 0x20 || ch == 0x7F; }
    static bool isC1Control(uint8_t ch) { return ch >= 0x80 && ch < 0xA0; }
    static bool isPrintable(uint8_t ch) { return ch >= 0x20 && ch < 0x7F; }
    static bool isParameter(uint8_t ch) { return (ch >= 0x30 && ch <= 0x3F); }
    static bool isIntermediate(uint8_t ch) { return (ch >= 0x20 && ch <= 0x2F); }
    static bool isFinal(uint8_t ch) { return (ch >= 0x40 && ch <= 0x7E); }
    static bool isPrivateMarker(uint8_t ch) { return ch == '<' || ch == '=' || ch == '>' || ch == '?'; }
};

} // namespace tx
