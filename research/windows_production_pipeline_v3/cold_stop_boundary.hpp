#pragma once
#include <cstdint>
#include <stdexcept>

// Included only by endpoint-free fixtures after windows.h. The fixture owns
// its keyboard/console input, just as it owns synthetic capture and touch.
// The production translation unit and executable retain the Win32 APIs.
namespace cold {
struct StopBoundary final {
    bool escape_held=false;
    DWORD console_event=0;
    bool console_pending=false;
    PHANDLER_ROUTINE handler=nullptr;
    std::uint64_t key_polls=0,handler_adds=0,handler_removes=0,console_deliveries=0;

    SHORT key_state(int key) {
        if(key!=VK_ESCAPE)throw std::runtime_error("cold stop boundary only permits Escape");
        ++key_polls;
        if(console_pending){
            console_pending=false;
            if(!handler||!handler(console_event))throw std::runtime_error("controlled console stop was not handled");
            ++console_deliveries;
        }
        return escape_held?static_cast<SHORT>(0x8000):0;
    }
    BOOL set_handler(PHANDLER_ROUTINE value,BOOL add) noexcept {
        if(!value)return FALSE;
        if(add){if(handler)return FALSE;handler=value;++handler_adds;return TRUE;}
        if(handler!=value)return FALSE;
        handler=nullptr;++handler_removes;return TRUE;
    }
};
inline StopBoundary stop_boundary;
inline SHORT WINAPI controlled_key_state(int key){return stop_boundary.key_state(key);}
inline BOOL WINAPI controlled_console_handler(PHANDLER_ROUTINE handler,BOOL add) noexcept {
    return stop_boundary.set_handler(handler,add);
}
}
