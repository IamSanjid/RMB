#include "macos_appkit.h"
#include "macos_native.h"

#include <algorithm>
#include <array>

std::shared_ptr<Native> Native::GetInstance() {
    static std::shared_ptr<Native> singleton_(MacOSNative::GetInstance());
    return singleton_;
}

MacOSNative* MacOSNative::GetInstance() {
    if (!instance_)
        instance_ = new MacOSNative();
    return instance_;
}

MacOSNative* MacOSNative::instance_ = nullptr;

static uint32_t HashRegKey(uint32_t key, uint32_t modmask) {
    return key * 0x7FFFu + modmask;
}

MacOSNative::MacOSNative() {
    app_kit_ = new AppKit();

    cg_handler = new CGEventHandler(MacOSNative::HotkeyHandler, MacOSNative::MouseButtonHandler);

    // ask for permissions...
    app_kit_->requestPermissions();
}

MacOSNative::~MacOSNative() {
    if (cg_handler != nullptr) {
        delete cg_handler;
        cg_handler = nullptr;
    }

    if (app_kit_ != nullptr) {
        delete app_kit_;
        app_kit_ = nullptr;
    }

    instance_ = nullptr;
}

void MacOSNative::RegisterHotKey(uint32_t key, uint32_t modifier) {
    fprintf(stdout, "Registering Hotkey: key=0x%x, modifier=0x%x\n", key, modifier);
    uint32_t hashRegkey = HashRegKey(key, modifier);

    registered_keys_[hashRegkey] = {
        .key = key,
        .modifier = modifier,
    };
}

void MacOSNative::UnregisterHotKey(uint32_t key, uint32_t modifier) {
    uint32_t hashRegkey = HashRegKey(key, modifier);
    if (!registered_keys_.contains(hashRegkey)) {
        return;
    }
    registered_keys_.erase(hashRegkey);
}

void MacOSNative::SendKeysDown(uint32_t* keys, size_t count) {
    for (size_t i = 0; i < count; i++) {
        SendKey(keys[i], true);
    }
}

void MacOSNative::SendKeysUp(uint32_t* keys, size_t count) {
    for (size_t i = 0; i < count; i++) {
        SendKey(keys[i], false);
    }
}

void MacOSNative::SetMousePos(int x, int y) {
    /* only focuses on the main display */
    cg_handler->SetMousePos(x, y);
}

void MacOSNative::GetMousePos(int* x_ret, int* y_ret) {
    float x;
    float y;
    app_kit_->getCursorPos(&x, &y);
    *x_ret = (int)x;
    *y_ret = (int)y;
}

NativeWindow MacOSNative::GetFocusedWindow() {
    return static_cast<NativeWindow>(app_kit_->activeProcessId());
}

bool MacOSNative::SetFocusOnWindow(const NativeWindow id) {
    return app_kit_->setFocusOnWindowId(static_cast<int>(id));
}

bool MacOSNative::IsMainWindowActive(const std::string& window_name) {
    return app_kit_->isMainWindowActive(window_name.c_str());
}

bool MacOSNative::SetFocusOnWindow(const std::string& window_name) {
    return app_kit_->setFocusOnWindow(window_name.c_str());
}

void MacOSNative::CursorHide(bool hide) {
    cg_handler->CursorHide(hide);
    app_kit_->cursorHide(hide);
}

void MacOSNative::Update() {
    cg_handler->Update();

    while (!mouse_button_evnets_.empty()) {
        EventBus::Instance().publish(mouse_button_evnets_.front());
        mouse_button_evnets_.pop();
    }

    while (!hotkey_events_.empty()) {
        EventBus::Instance().publish(hotkey_events_.front());
        hotkey_events_.pop();
    }
}

bool MacOSNative::HotkeyHandler(uint32_t keycode, uint32_t modifier) {
    uint32_t hash = HashRegKey(keycode, modifier);
    if (!MacOSNative::instance_->registered_keys_.contains(hash)) {
        return false;
    }
    MacOSNative::instance_->hotkey_events_.push(HotkeyEvent(keycode, modifier));
    return true;
}

bool MacOSNative::MouseButtonHandler(uint32_t key, bool is_down, int x, int y) {
    MacOSNative::instance_->mouse_button_evnets_.push(MouseButtonEvent(key, is_down, x, y));
    return false;
}

void MacOSNative::SendKey(uint32_t key, bool is_down) {
    if (cg_handler) {
        if (!cg_handler->SendKey(0, key, is_down)) {
            fprintf(stderr, "failed to send key...\n");
        }
    }
}

void MacOSNative::SendChar(char32_t chr, uint32_t key, bool is_down) {
    if (cg_handler) {
        if (!cg_handler->SendKey(chr, 0, is_down)) {
            fprintf(stderr, "failed to send char...\n");
        }
    }
}