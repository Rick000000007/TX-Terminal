#include "tx/parser.hpp"
#include <cctype>

namespace tx {

// Control character definitions
constexpr uint8_t NUL = 0x00;  // Null
constexpr uint8_t SOH = 0x01;  // Start of Heading
constexpr uint8_t STX = 0x02;  // Start of Text
constexpr uint8_t ETX = 0x03;  // End of Text
constexpr uint8_t EOT = 0x04;  // End of Transmission
constexpr uint8_t ENQ = 0x05;  // Enquiry
constexpr uint8_t ACK = 0x06;  // Acknowledge
constexpr uint8_t BEL = 0x07;  // Bell
constexpr uint8_t BS  = 0x08;  // Backspace
constexpr uint8_t HT  = 0x09;  // Horizontal Tab
constexpr uint8_t LF  = 0x0A;  // Line Feed
constexpr uint8_t VT  = 0x0B;  // Vertical Tab
constexpr uint8_t FF  = 0x0C;  // Form Feed
constexpr uint8_t CR  = 0x0D;  // Carriage Return
constexpr uint8_t SO  = 0x0E;  // Shift Out
constexpr uint8_t SI  = 0x0F;  // Shift In
constexpr uint8_t DLE = 0x10;  // Data Link Escape
constexpr uint8_t DC1 = 0x11;  // Device Control 1 (XON)
constexpr uint8_t DC2 = 0x12;  // Device Control 2
constexpr uint8_t DC3 = 0x13;  // Device Control 3 (XOFF)
constexpr uint8_t DC4 = 0x14;  // Device Control 4
constexpr uint8_t NAK = 0x15;  // Negative Acknowledge
constexpr uint8_t SYN = 0x16;  // Synchronous Idle
constexpr uint8_t ETB = 0x17;  // End of Transmission Block
constexpr uint8_t CAN = 0x18;  // Cancel
constexpr uint8_t EM  = 0x19;  // End of Medium
constexpr uint8_t SUB = 0x1A;  // Substitute
constexpr uint8_t ESC = 0x1B;  // Escape
constexpr uint8_t FS  = 0x1C;  // File Separator
constexpr uint8_t GS  = 0x1D;  // Group Separator
constexpr uint8_t RS  = 0x1E;  // Record Separator
constexpr uint8_t US  = 0x1F;  // Unit Separator
constexpr uint8_t DEL = 0x7F;  // Delete

// Intermediate bytes
constexpr uint8_t SP  = 0x20;  // Space
constexpr uint8_t DCS = 0x90;  // Device Control String (C1)
constexpr uint8_t SOS = 0x98;  // Start of String (C1)
constexpr uint8_t CSI = 0x9B;  // Control Sequence Introducer (C1)
constexpr uint8_t ST  = 0x9C;  // String Terminator (C1)
constexpr uint8_t OSC = 0x9D;  // Operating System Command (C1)
constexpr uint8_t PM  = 0x9E;  // Privacy Message (C1)
constexpr uint8_t APC = 0x9F;  // Application Program Command (C1)

Parser::Parser() {
    reset();
}

void Parser::reset() {
    state_ = State::Ground;
    clearParams();
    intermediate_ = 0;
    osc_buffer_.clear();
    osc_cmd_ = 0;
    dcs_cmd_ = 0;
}

void Parser::parse(const uint8_t* data, size_t len, const ParserActions& actions) {
    for (size_t i = 0; i < len; ++i) {
        uint8_t ch = data[i];
        
        switch (state_) {
            case State::Ground:
                handleGround(ch, actions);
                break;
                
            case State::Escape:
                if (ch == '[') {
                    state_ = State::CSI_Entry;
                    clearParams();
                } else if (ch == ']') {
                    state_ = State::OSC_String;
                    osc_buffer_.clear();
                    osc_cmd_ = 0;
                } else if (ch == 'P') {
                    state_ = State::DCS_Entry;
                    clearParams();
                } else if (ch == '_') {
                    state_ = State::SOS_PM_APC_String;
                } else if (ch == '^') {
                    state_ = State::SOS_PM_APC_String;
                } else if (ch == '\\') {
                    state_ = State::Ground;  // ST (String Terminator)
                } else if (isIntermediate(ch)) {
                    intermediate_ = ch;
                    state_ = State::EscapeIntermediate;
                } else if (isFinal(ch)) {
                    // ESC + final byte
                    if (actions.esc) {
                        actions.esc(intermediate_, ch);
                    }
                    intermediate_ = 0;
                    state_ = State::Ground;
                } else if (ch >= 0x30 && ch <= 0x7E) {
                    // ESC + intermediate + final
                    if (actions.esc) {
                        actions.esc(intermediate_, ch);
                    }
                    intermediate_ = 0;
                    state_ = State::Ground;
                } else if (isC0Control(ch) && ch != ESC) {
                    // Execute C0 controls in escape state
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                } else {
                    // Invalid, return to ground
                    state_ = State::Ground;
                }
                break;
                
            case State::EscapeIntermediate:
                if (isIntermediate(ch)) {
                    intermediate_ = ch;
                } else if (isFinal(ch)) {
                    if (actions.esc) {
                        actions.esc(intermediate_, ch);
                    }
                    intermediate_ = 0;
                    state_ = State::Ground;
                } else if (ch == ESC) {
                    state_ = State::Escape;
                } else if (isC0Control(ch)) {
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                } else {
                    state_ = State::Ground;
                }
                break;
                
            case State::CSI_Entry:
                handleCSIEntry(ch, actions);
                break;
                
            case State::CSI_Param:
                if (isParameter(ch)) {
                    if (ch >= '0' && ch <= '9') {
                        param_has_value_ = true;
                        // Accumulate digit (params_[param_count_] = params_[param_count_] * 10 + (ch - '0'))
                        // But we store in params_ directly
                        if (param_count_ < MAX_PARAMS) {
                            params_[param_count_] = params_[param_count_] * 10 + (ch - '0');
                        }
                    } else if (ch == ':') {
                        // Sub-parameter separator (treat as separator)
                        paramPush(0);
                    } else if (ch == ';') {
                        paramPush(0);
                    } else if (ch < 0x3C) {
                        // ':', '<', '=', '>', '?'
                        state_ = State::CSI_Ignore;
                    } else {
                        // Intermediate
                        intermediate_ = ch;
                        state_ = State::CSI_Intermediate;
                    }
                } else if (isIntermediate(ch)) {
                    intermediate_ = ch;
                    state_ = State::CSI_Intermediate;
                } else if (isFinal(ch)) {
                    paramPush(0);
                    dispatchCSI(ch, actions);
                    state_ = State::Ground;
                } else if (ch == ESC) {
                    state_ = State::Escape;
                } else if (isC0Control(ch)) {
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                } else {
                    state_ = State::CSI_Ignore;
                }
                break;
                
            case State::CSI_Intermediate:
                if (isIntermediate(ch)) {
                    intermediate_ = ch;
                } else if (isFinal(ch)) {
                    paramPush(0);
                    dispatchCSI(ch, actions);
                    state_ = State::Ground;
                } else if (ch == ESC) {
                    state_ = State::Escape;
                } else if (isC0Control(ch)) {
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                } else {
                    state_ = State::CSI_Ignore;
                }
                break;
                
            case State::CSI_Ignore:
                if (isFinal(ch)) {
                    state_ = State::Ground;
                } else if (ch == ESC) {
                    state_ = State::Escape;
                } else if (isC0Control(ch)) {
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                }
                break;
                
            case State::DCS_Entry:
                if (isParameter(ch)) {
                    state_ = State::DCS_Param;
                    if (ch >= '0' && ch <= '9') {
                        param_has_value_ = true;
                        if (param_count_ < MAX_PARAMS) {
                            params_[param_count_] = ch - '0';
                        }
                    } else if (ch == ';') {
                        paramPush(0);
                    }
                } else if (isIntermediate(ch)) {
                    intermediate_ = ch;
                    state_ = State::DCS_Intermediate;
                } else if (isFinal(ch)) {
                    paramPush(0);
                    dcs_cmd_ = ch;
                    if (actions.dcs_hook) {
                        actions.dcs_hook(dcs_cmd_, params_, param_count_);
                    }
                    state_ = State::DCS_Passthrough;
                } else if (ch == ESC) {
                    state_ = State::Escape;
                } else if (isC0Control(ch)) {
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                } else {
                    state_ = State::DCS_Ignore;
                }
                break;
                
            case State::DCS_Param:
                if (isParameter(ch)) {
                    if (ch >= '0' && ch <= '9') {
                        if (param_count_ < MAX_PARAMS) {
                            params_[param_count_] = params_[param_count_] * 10 + (ch - '0');
                        }
                    } else if (ch == ';') {
                        paramPush(0);
                    }
                } else if (isIntermediate(ch)) {
                    intermediate_ = ch;
                    state_ = State::DCS_Intermediate;
                } else if (isFinal(ch)) {
                    paramPush(0);
                    dcs_cmd_ = ch;
                    if (actions.dcs_hook) {
                        actions.dcs_hook(dcs_cmd_, params_, param_count_);
                    }
                    state_ = State::DCS_Passthrough;
                } else if (ch == ESC) {
                    state_ = State::Escape;
                } else if (isC0Control(ch)) {
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                } else {
                    state_ = State::DCS_Ignore;
                }
                break;
                
            case State::DCS_Intermediate:
                if (isIntermediate(ch)) {
                    intermediate_ = ch;
                } else if (isFinal(ch)) {
                    paramPush(0);
                    dcs_cmd_ = ch;
                    if (actions.dcs_hook) {
                        actions.dcs_hook(dcs_cmd_, params_, param_count_);
                    }
                    state_ = State::DCS_Passthrough;
                } else if (ch == ESC) {
                    state_ = State::Escape;
                } else if (isC0Control(ch)) {
                    if (actions.execute) {
                        actions.execute(ch);
                    }
                } else {
                    state_ = State::DCS_Ignore;
                }
                break;
                
            case State::DCS_Passthrough:
                handleDCSPassthrough(ch, actions);
                break;
                
            case State::DCS_Ignore:
                if (ch == ESC) {
                    state_ = State::Escape;
                } else if (ch == ST || ch == 0x9C) {
                    state_ = State::Ground;
                }
                break;
                
            case State::OSC_String:
                handleOSC(ch, actions);
                break;
                
            case State::SOS_PM_APC_String:
                if (ch == ESC) {
                    state_ = State::Escape;
                } else if (ch == ST || ch == 0x9C) {
                    state_ = State::Ground;
                }
                // Otherwise, collect string data
                break;
        }
    }
}

void Parser::parse(std::string_view data, const ParserActions& actions) {
    parse(reinterpret_cast<const uint8_t*>(data.data()), data.size(), actions);
}

void Parser::handleGround(uint8_t ch, const ParserActions& actions) {
    if (ch == ESC) {
        state_ = State::Escape;
    } else if (ch >= 0x80 && ch <= 0x9F) {
        // C1 controls
        switch (ch) {
            case CSI:
                state_ = State::CSI_Entry;
                clearParams();
                break;
            case DCS:
                state_ = State::DCS_Entry;
                clearParams();
                break;
            case OSC:
                state_ = State::OSC_String;
                osc_buffer_.clear();
                osc_cmd_ = 0;
                break;
            case ST:
                // String terminator, stay in ground
                break;
            case SOS:
            case PM:
            case APC:
                state_ = State::SOS_PM_APC_String;
                break;
            default:
                // Other C1 controls - execute if handler provided
                if (actions.execute) {
                    actions.execute(ch);
                }
                break;
        }
    } else if (isC0Control(ch)) {
        if (actions.execute) {
            actions.execute(ch);
        }
    } else {
        // Printable character
        if (actions.print) {
            actions.print(ch);
        }
    }
}

void Parser::handleCSIEntry(uint8_t ch, const ParserActions& actions) {
    if (isParameter(ch)) {
        state_ = State::CSI_Param;
        if (ch >= '0' && ch <= '9') {
            param_has_value_ = true;
            if (param_count_ < MAX_PARAMS) {
                params_[param_count_] = ch - '0';
            }
        } else if (ch == ';') {
            paramPush(0);
        } else if (isPrivateMarker(ch)) {
            // Private marker - store as intermediate
            intermediate_ = ch;
        } else {
            state_ = State::CSI_Ignore;
        }
    } else if (isIntermediate(ch)) {
        intermediate_ = ch;
        state_ = State::CSI_Intermediate;
    } else if (isFinal(ch)) {
        paramPush(0);
        dispatchCSI(ch, actions);
        state_ = State::Ground;
    } else if (ch == ESC) {
        state_ = State::Escape;
    } else if (isC0Control(ch)) {
        if (actions.execute) {
            actions.execute(ch);
        }
    } else {
        state_ = State::CSI_Ignore;
    }
}

void Parser::handleDCSPassthrough(uint8_t ch, const ParserActions& actions) {
    if (ch == ESC) {
        // Check if next char is backslash (ST)
        state_ = State::Escape;
    } else if (ch == ST || ch == 0x9C) {
        if (actions.dcs_unhook) {
            actions.dcs_unhook();
        }
        state_ = State::Ground;
    } else if (isC0Control(ch)) {
        // C0 controls are passed through in DCS
        if (actions.dcs_put) {
            actions.dcs_put(&ch, 1);
        }
    } else {
        // Pass through data
        if (actions.dcs_put) {
            actions.dcs_put(&ch, 1);
        }
    }
}

void Parser::handleOSC(uint8_t ch, const ParserActions& actions) {
    if (ch == ESC) {
        state_ = State::Escape;
    } else if (ch == BEL) {
        // BEL terminates OSC
        dispatchOSC(actions);
        state_ = State::Ground;
    } else if (ch == ST || ch == 0x9C) {
        dispatchOSC(actions);
        state_ = State::Ground;
    } else if (ch >= 0x20 && ch < 0x7F) {
        // Collect OSC data
        if (osc_buffer_.empty() && ch >= '0' && ch <= '9') {
            // First digit(s) are the OSC command number
            osc_cmd_ = osc_cmd_ * 10 + (ch - '0');
        } else if (osc_buffer_.empty() && ch == ';') {
            // Semicolon separates command from data
            osc_buffer_ += ';';
        } else {
            osc_buffer_ += static_cast<char>(ch);
        }
    } else if (ch >= 0x80) {
        // High bytes in OSC (UTF-8 continuation)
        osc_buffer_ += static_cast<char>(ch);
    }
    // C0 controls other than BEL are ignored in OSC
}

void Parser::clearParams() {
    param_count_ = 0;
    param_has_value_ = false;
    intermediate_ = 0;
    for (int i = 0; i < MAX_PARAMS; ++i) {
        params_[i] = 0;
    }
}

void Parser::paramPush(int value) {
    if (param_count_ < MAX_PARAMS) {
        if (!param_has_value_) {
            params_[param_count_] = value;
        }
        param_count_++;
        param_has_value_ = false;
    }
}

void Parser::dispatchCSI(uint8_t cmd, const ParserActions& actions) {
    if (actions.csi) {
        actions.csi(cmd, params_, param_count_);
    }
}

void Parser::dispatchOSC(const ParserActions& actions) {
    if (actions.osc) {
        // Remove leading semicolon if present
        std::string_view data(osc_buffer_);
        if (!data.empty() && data[0] == ';') {
            data = data.substr(1);
        }
        actions.osc(osc_cmd_, data);
    }
}

} // namespace tx
