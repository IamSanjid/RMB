#pragma once

#ifdef _MSC_VER
#if !defined(_WIN64)
#error Only 64bit is supported.
#endif
#endif

#include <cstdint>
#include <memory>
#include <string>
#include <bitset>

#include "EventSystem.h"

struct HotkeyEvent : Event {
    HotkeyEvent(uint32_t key, uint32_t modifier) : key(key), modifier(modifier){};
    uint32_t key;
    uint32_t modifier;
};

struct MouseButtonEvent : Event {
    MouseButtonEvent(uint32_t key, bool is_pressed, int x, int y)
        : key(key), is_pressed(is_pressed), x(x), y(y){};
    uint32_t key;
    bool is_pressed;
    int x;
    int y;
};

const uint32_t MOUSE_LBUTTON = 0x1;
const uint32_t MOUSE_RBUTTON = 0x2;
const uint32_t MOUSE_MBUTTON = 0x3;

#ifdef _WIN32
using NativeWindow = void*;
#else
#if defined(__APPLE__) || defined(__MACH__)
using NativeWindow = __int32_t; // __darwin_pid_t
#else
using NativeWindow = unsigned long;
#endif
#endif

static const size_t MAX_KEYBOARD_SCAN_CODE = 0xff;
using KeysBitset = std::bitset<MAX_KEYBOARD_SCAN_CODE>;

class Native {
public:
    Native(){};
    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;

    static std::shared_ptr<Native> GetInstance();

    virtual ~Native() = default;
    /* the keys are expected to be the scan/os codes */
    virtual void RegisterHotKey(uint32_t key, uint32_t modifier) = 0;
    virtual void UnregisterHotKey(uint32_t key, uint32_t modifier) = 0;
    virtual void SendKeysDown(uint32_t* keys, size_t count) = 0;
    virtual void SendKeysUp(uint32_t* keys, size_t count) = 0;
    inline void SendKeysBitsetDown(const KeysBitset& key_map) {
        const size_t count = key_map.count();
        if (count == 0) {
            return;
        }
        uint32_t keys[MAX_KEYBOARD_SCAN_CODE]{};
        size_t curr_inp_idx = 0;
        // iterting over 0xff items is super fast
        for (uint32_t i = 0; i < MAX_KEYBOARD_SCAN_CODE; i++) {
            if (key_map[i]) {
                keys[curr_inp_idx++] = i;
            }
        }
        SendKeysDown(keys, curr_inp_idx);
    }
    inline void SendKeysBitsetUp(const KeysBitset& key_map) {
        const size_t count = key_map.count();
        if (count == 0) {
            return;
        }
        uint32_t keys[MAX_KEYBOARD_SCAN_CODE]{};
        size_t curr_inp_idx = 0;
        // iterting over 0xff items is super fast
        for (uint32_t i = 0; i < MAX_KEYBOARD_SCAN_CODE; i++) {
            if (key_map[i]) {
                keys[curr_inp_idx] = i;
                curr_inp_idx++;
            }
        }
        SendKeysUp(keys, curr_inp_idx);
    }
    /* (0, 0) should be at the top left corner of the main monitor/screen */
    virtual void SetMousePos(int x, int y) = 0;
    virtual void GetMousePos(int* x_ret, int* y_ret) = 0;

    /* optional */
    virtual NativeWindow GetFocusedWindow() = 0;
    virtual bool SetFocusOnWindow(const NativeWindow window) = 0;
    /* if some windows's name/title has substr of the specified name the following task/result will be performed/returned */
    virtual bool IsMainWindowActive(const std::string& window_name) = 0;
    virtual bool SetFocusOnWindow(const std::string& window_name) = 0;

    virtual void CursorHide(bool hide) = 0;

    /* should not block the current thread */
    virtual void Update() = 0;
};