#pragma once

#include "native.h"

#include <unordered_map>
#include <vector>
#include <queue>

class AppKit;

class MacOSNative : public Native {
public:
    MacOSNative();

    MacOSNative(const MacOSNative&) = delete;
    MacOSNative& operator=(const MacOSNative&) = delete;

    static MacOSNative* GetInstance();

    ~MacOSNative() override;

    void RegisterHotKey(uint32_t key, uint32_t modifier) override;
    void UnregisterHotKey(uint32_t key, uint32_t modifier) override;
    void SendKeysDown(uint32_t* keys, size_t count) override;
    void SendKeysUp(uint32_t* keys, size_t count) override;
    void SetMousePos(int x, int y) override;
    void GetMousePos(int* x_ret, int* y_ret) override;
    NativeWindow GetFocusedWindow() override;
    bool SetFocusOnWindow(const NativeWindow) override;
    bool IsMainWindowActive(const std::string& window_name) override;
    bool SetFocusOnWindow(const std::string& window_name) override;
    void CursorHide(bool hide) override;
    void Update() override;

private:
    class KeyboardLayout;
    class CGEventHandler;

    struct RegKey {
        uint32_t key, modifier;
    };

    struct ScanCodeInfo {
        char32_t unichar;
        uint32_t modifier;
    };

    static bool HotkeyHandler(uint32_t keycode, uint32_t modifier);
    static bool MouseButtonHandler(uint32_t key, bool is_down, int x, int y);

    void SendKey(uint32_t key, bool is_down);
    void SendChar(char32_t chr, uint32_t key, bool is_down);

    static MacOSNative* instance_;

    AppKit* app_kit_ = nullptr;
    CGEventHandler* cg_handler = nullptr;
    KeyboardLayout* keyboard_layout_ = nullptr;
    std::unordered_map<uint32_t, ScanCodeInfo> scancode_infos_;
    std::unordered_map<uint32_t, RegKey> registered_keys_;

    std::queue<MouseButtonEvent> mouse_button_evnets_;
    std::queue<HotkeyEvent> hotkey_events_;
};