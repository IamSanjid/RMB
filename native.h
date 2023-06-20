#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "EventSystem.h"

struct HotkeyEvent : Event {
    HotkeyEvent(uint32_t key, uint32_t modifier) : key_(key), modifier_(modifier){};
    uint32_t key_;
    uint32_t modifier_;
};

struct MouseButtonEvent : Event {
    MouseButtonEvent(uint32_t key, bool is_pressed, int _x, int _y)
        : key_(key), is_pressed_(is_pressed), x(_x), y(_y){};
    uint32_t key_;
    bool is_pressed_;
    int x;
    int y;
};

const uint32_t MOUSE_LBUTTON = 0x1;
const uint32_t MOUSE_RBUTTON = 0x2;
const uint32_t MOUSE_MBUTTON = 0x3;

class Native {
public:
    Native(){};
    Native(const Native&) = delete;
    Native& operator=(const Native&) = delete;

    static std::shared_ptr<Native> GetInstance();

    virtual ~Native() = default;
    /* the keys are expected to be the scan code for the specific key on the specific platform */
    virtual void RegisterHotKey(uint32_t key, uint32_t modifier) = 0;
    virtual void UnregisterHotKey(uint32_t key, uint32_t modifier) = 0;
    virtual void SendKeysDown(uint32_t* keys, size_t count) = 0;
    virtual void SendKeysUp(uint32_t* keys, size_t count) = 0;
    virtual void SetMousePos(int x, int y) = 0;
    virtual void GetMousePos(int* x_ret, int* y_ret) = 0;
    /* performs task on any window if it's name has substr of the specified name */
    virtual bool IsMainWindowActive(const std::string& window_name) = 0;
    /* optional */
    virtual bool SetFocusOnWindow(const std::string& window_name) = 0;
    virtual void CursorHide(bool hide) = 0;
    /* should not block the current thread */
    virtual void Update() = 0;
};