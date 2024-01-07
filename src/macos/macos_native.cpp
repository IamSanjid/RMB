#include "macos_native.h"
#include "macos_appkit.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include <algorithm>
#include <array>

std::shared_ptr<Native> Native::GetInstance()
{
    static std::shared_ptr<Native> singleton_(MacOSNative::GetInstance());
    return singleton_;
}

MacOSNative *MacOSNative::GetInstance()
{
    if (!instance_)
        instance_ = new MacOSNative();
    return instance_;
}

MacOSNative *MacOSNative::instance_ = nullptr;

/*constexpr auto KEY_PRESS_TOTAL_WAIT_TIME_MS = 10'000;
constexpr auto KEY_PRESS_LOOP_WAIT_TIME_MS = 1;

constexpr std::array KEY_CODES_AFFECTING_EVENT_FLAGS{
    std::make_pair(kVK_Command, static_cast<uint64_t>(NX_DEVICELCMDKEYMASK)),
    std::make_pair(kVK_Control, static_cast<uint64_t>(NX_DEVICELCTLKEYMASK)),
    std::make_pair(kVK_Function, static_cast<uint64_t>(NX_SECONDARYFNMASK)),
    std::make_pair(kVK_Option, static_cast<uint64_t>(NX_DEVICELALTKEYMASK)),
    std::make_pair(kVK_Shift, static_cast<uint64_t>(NX_DEVICELSHIFTKEYMASK)),
    std::make_pair(kVK_RightCommand, static_cast<uint64_t>(NX_DEVICERCMDKEYMASK)),
    std::make_pair(kVK_RightControl, static_cast<uint64_t>(NX_DEVICERCTLKEYMASK)),
    std::make_pair(kVK_RightOption, static_cast<uint64_t>(NX_DEVICERALTKEYMASK)),
    std::make_pair(kVK_RightShift, static_cast<uint64_t>(NX_DEVICERSHIFTKEYMASK)),
    std::make_pair(kVK_CapsLock, static_cast<uint64_t>(NX_ALPHASHIFTMASK)),
};
constexpr auto KEY_CODES_AFFECTING_EVENT_FLAGS_MIN =
    std::min_element(KEY_CODES_AFFECTING_EVENT_FLAGS.begin(), KEY_CODES_AFFECTING_EVENT_FLAGS.end(),
                     [](auto lhs, auto rhs) { return lhs.first < rhs.first; }) -> first;

constexpr auto KEY_CODES_AFFECTING_EVENT_FLAGS_MAX =
    std::max_element(KEY_CODES_AFFECTING_EVENT_FLAGS.begin(), KEY_CODES_AFFECTING_EVENT_FLAGS.end(),
                     [](auto lhs, auto rhs) { return lhs.first < rhs.first; }) -> first;*/

class MacOSNative::KeyboardLayout{
#define ReleaseRet(ref) do { ::CFRelease(ref); return; } while(0)
#define ReleaseWithResult(ref, result) do { ::CFRelease(ref); return result; } while(0)
private:
    static constexpr int MAX_KEYBOARD_LAYOUT_CHAR_CODE = 127;
    static constexpr std::array SPECIAL_CHARACTERS_TO_KEYCODES{
        std::make_pair(U'\n', (uint32_t)kVK_Return),
    };

    CFDataRef keyboard_layout_data_ = nullptr;
    CGEventSourceRef event_source_ = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);

public:
    void read_layout(std::unordered_map<uint32_t, ScanCodeInfo>& info_map) {
        auto keyboard = TISCopyCurrentKeyboardInputSource();
        const auto *layout_data = static_cast<CFDataRef>(
            TISGetInputSourceProperty(keyboard, kTISPropertyUnicodeKeyLayoutData));

        if (layout_data == keyboard_layout_data_) {
            ReleaseRet(keyboard);
        }

        const auto *layout = reinterpret_cast<const UCKeyboardLayout *>(CFDataGetBytePtr(layout_data));
        auto kbd_type = LMGetKbdType();

        uint32_t keys_down = 0;
        std::array<UniChar, 4> chars{};
        UniCharCount length = 0;

        info_map.clear();
        static constexpr auto SHIFT_MASK = (static_cast<UInt32>(shiftKey) >> 8U) & 0xFFU;
        static constexpr std::array MODIFIER_STATES{
            std::make_pair(0U, 0U), std::make_pair(SHIFT_MASK, (uint32_t)kVK_Shift),
            // other modifiers cause issues in Terminal and similar
        };

        for (int code = 0; code <= MAX_KEYBOARD_LAYOUT_CHAR_CODE; code++) {
            for (auto [mod_state, modifier] : MODIFIER_STATES) {
                auto status = UCKeyTranslate(layout, code, kUCKeyActionDown, mod_state, kbd_type,
                                             kUCKeyTranslateNoDeadKeysBit, &keys_down, 4, &length,
                                             chars.data());
                auto ch = chars[0];
                if (status == noErr && length == 1 && ch && !info_map.count(ch)) {
                    ScanCodeInfo scancode_info{};
                    scancode_info.unichar = ch;
                    scancode_info.modifier = modifier;
                    info_map.emplace(code, scancode_info);
                }
            }
        }

        for (auto [character, code] : SPECIAL_CHARACTERS_TO_KEYCODES) {
            if (!info_map.count(code)) {
                ScanCodeInfo sc{};
                sc.unichar = character;
                info_map.emplace(code, sc);
            }
        }

        keyboard_layout_data_ = layout_data;
        ReleaseRet(keyboard);
    }

    bool send_key(char32_t chr, uint32_t code, bool down) {
        CGEventRef keyEvent = CGEventCreateKeyboardEvent(event_source_, (CGKeyCode)code, down);
        if (!keyEvent) {
            return false;
        }
        if (chr) {
            CFStringRef cfstr = CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<UInt8 *>(&chr), sizeof(chr), kCFStringEncodingUTF32LE, false);
            if (!cfstr) {
                ReleaseWithResult(keyEvent, false);
            }

            auto length = CFStringGetLength(cfstr);
            std::vector<UniChar> utf16_data(length);
            CFStringGetCharacters(cfstr, CFRangeMake(0, length), utf16_data.data());
            CGEventKeyboardSetUnicodeString(keyEvent, utf16_data.size(), utf16_data.data());

            CFRelease(cfstr);
        }

        /*auto expect_event_flags_change = code >= KEY_CODES_AFFECTING_EVENT_FLAGS_MIN &&
                                         code <= KEY_CODES_AFFECTING_EVENT_FLAGS_MAX;
        auto event_flags_change_counter =
            expect_event_flags_change ? CGEventSourceCounterForEventType(
                                            kCGEventSourceStateHIDSystemState, kCGEventFlagsChanged)
                                      : 0;*/

        CGEventPost(kCGHIDEventTap, keyEvent);
        /*auto start_time = std::chrono::system_clock::now();
        while (true) {
            if (expect_event_flags_change) {
                // Modifier keys state is not always reflected in the event source,
                // CGEventSourceKeyState, CGEventSourceFlagsState, etc...
                // sometimes return the old value when the mouse is moved during typing.
                // For these events we'll just wait for the flag change event.
                auto counter_now = CGEventSourceCounterForEventType(
                    kCGEventSourceStateHIDSystemState, kCGEventFlagsChanged);
                if (counter_now > event_flags_change_counter) {
                    break;
                }
            } else {
                auto key_state = CGEventSourceKeyState(kCGEventSourceStateHIDSystemState, code);
                if (key_state == down) {
                    break;
                }
            }
            auto elapsed = std::chrono::system_clock::now() - start_time;
            auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
            if (wait_ms.count() > KEY_PRESS_TOTAL_WAIT_TIME_MS) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(KEY_PRESS_LOOP_WAIT_TIME_MS));
        }*/

        CFRelease(keyEvent);
        return true;
    }

    CGEventSourceRef get_event_source() const { return event_source_; }

    ~KeyboardLayout() {
        if (keyboard_layout_data_) {
            ReleaseRet(keyboard_layout_data_);
        }
    }
};

static CGEventFlags MacOSModifierToEventFlag(uint32_t modifier)
{
    CGEventFlags nativeEventFlag = 0;
    if (modifier == kVK_RightControl || modifier == kVK_Control)
    {
        nativeEventFlag = kCGEventFlagMaskControl;
    }
    else if (modifier == kVK_RightShift || modifier == kVK_Shift)
    {
        nativeEventFlag = kCGEventFlagMaskShift;
    }
    else if (modifier == kVK_Command)
    {
        nativeEventFlag = kCGEventFlagMaskCommand;
    }
    else if (modifier == kVK_Option || modifier == kVK_RightOption)
    {
        nativeEventFlag = kCGEventFlagMaskAlternate;
    }
    return nativeEventFlag;
}

static uint32_t MacOSEventFlagsToModifier(CGEventFlags flags) {
    // If only one modifier is flag is set then it works the best
    static constexpr uint64_t CONTROL = 0x40101;
    static constexpr uint64_t RIGHT_CONTROL = 0x42100;
    static constexpr uint64_t COMMAND = 0x100108;
    static constexpr uint64_t RIGHT_COMMAND = 0x100110;
    static constexpr uint64_t OPTION = 0x80120;
    static constexpr uint64_t RIGHT_OPTION = 0x80140;
    static constexpr uint64_t SHIFT = 0x20102;
    static constexpr uint64_t RIGHT_SHIFT = 0x20104;

    static constexpr std::array flags_to_modifier {
        std::make_pair(CONTROL, kVK_Control),
        std::make_pair(RIGHT_CONTROL, kVK_RightControl),
        std::make_pair(COMMAND, kVK_Command),
        std::make_pair(RIGHT_COMMAND, kVK_RightCommand),
        std::make_pair(OPTION, kVK_Option),
        std::make_pair(RIGHT_OPTION, kVK_RightOption),
        std::make_pair(SHIFT, kVK_Shift),
        std::make_pair(RIGHT_SHIFT, kVK_RightShift),
    };

    for (auto [flag, key] : flags_to_modifier) {
        if ((flags & flag) == flag) {
            return key;
        }
    }

    return 0;
}

static uint32_t HashRegKey(uint32_t key, uint32_t modmask) {
    return key * 0x7FFFu + modmask;
}

static void handle_pending_events() {
    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource) {
        // handle all pending events
        // otherwise native_frontmost_app doesn't return correct results
    }
}

class MacOSNative::CGEventHandler {
private:
    CFMachPortRef eventTap = nullptr;
    CFRunLoopSourceRef runLoopSource = nullptr;

    static CGEventRef CGEventCallBack(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *user_ptr) {
        switch (type) {
            case kCGEventKeyDown: {
                CGEventFlags modifierFlags = CGEventGetFlags(event);
                uint32_t keycode = (uint32_t)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);
                uint32_t modifier = MacOSEventFlagsToModifier(modifierFlags);
#ifdef _DEBUG
                printf("[kCGEventKeyDown] key=0x%x, modifier=0x%x, modifierFlags=0x%llx\n", keycode, modifier, modifierFlags);
#endif
                if (modifierFlags == kCGEventFlagMaskNonCoalesced) {
                    return event;
                }

                if (MacOSNative::HotkeyHandler(keycode, modifier)) {
                    // Handled request delete...
                    auto source = CGEventCreateSourceFromEvent(event);
                    auto upModifier = CGEventCreateKeyboardEvent(source, (CGKeyCode)modifier, false);
                    auto newEvent = CGEventCreateKeyboardEvent(source, (CGKeyCode)keycode, false);
                    if (!newEvent || !upModifier) {
                        break;
                    }
                    CGEventSetFlags(newEvent, kCGEventFlagMaskNonCoalesced);
                    CGEventSetFlags(upModifier, kCGEventFlagMaskNonCoalesced);

                    CGEventTapPostEvent(proxy, upModifier);
                    
                    CFRelease(source);
                    CFRelease(upModifier);

                    return newEvent;
                }
                break;
            }
            case kCGEventTapDisabledByTimeout:
            case kCGEventTapDisabledByUserInput: {
                if (user_ptr != nullptr) {
                    CGEventHandler* _this = (CGEventHandler*)user_ptr;
                    CGEventTapEnable(_this->eventTap, true);
                }
                break;
            }
            default: {
                CGPoint location = CGEventGetLocation(event);
                uint32_t key = 0;
                int x = (int)abs(location.x);
                int y = (int)abs(location.y);
                bool is_pressed = true;
                switch (type)
                {
                case kCGEventLeftMouseDown:
                case kCGEventLeftMouseUp:
                    key = MOUSE_LBUTTON;
                    break;
                case kCGEventRightMouseDown:
                case kCGEventRightMouseUp:
                    key = MOUSE_RBUTTON;
                    break;         
                case kCGEventOtherMouseDown:
                case kCGEventOtherMouseUp:
                    key = MOUSE_MBUTTON;
                    break;
                default:
                    return event;
                }

                switch (type)
                {
                case kCGEventLeftMouseUp:
                case kCGEventRightMouseUp:
                case kCGEventOtherMouseUp:
                    is_pressed = false;
                    break;
                default: break;
                }

                MacOSNative::MouseButtonHandler(key, is_pressed, x, y);
                break;
            }
        }
        return event;
    }

public:
    CGEventHandler() {
        CGEventMask eventMask = (1 << kCGEventKeyDown) | (1 << kCGEventLeftMouseDown) | (1 << kCGEventLeftMouseUp) | (1 << kCGEventRightMouseDown) | (1 << kCGEventRightMouseUp) | (1 << kCGEventOtherMouseDown) | (1 << kCGEventOtherMouseUp);
        eventTap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, eventMask, CGEventHandler::CGEventCallBack, nullptr);
        if (!eventTap) {
            fprintf(stderr, "failed to create event tap\n");
            return;
        }
    }

    ~CGEventHandler() {
        if (eventTap) {
            CGEventTapEnable(eventTap, false);
            CFRelease(eventTap);
            if (runLoopSource) {
                CFRelease(runLoopSource);
            }
        }

        eventTap = nullptr;
        runLoopSource = nullptr;
    }

    void Update() {
        if (!runLoopSource) {
            runLoopSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);

            CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);
            CGEventTapEnable(eventTap, true);
        }

        handle_pending_events();
        //CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    }
};

MacOSNative::MacOSNative()
{
    app_kit_ = new AppKit();
    keyboard_layout_ = new KeyboardLayout();

    cg_handler = new CGEventHandler();

    keyboard_layout_->read_layout(scancode_infos_);

    // ask for permissions...
    app_kit_->enableAccessibility();
    CGRequestListenEventAccess();
    CGRequestPostEventAccess();
}

MacOSNative::~MacOSNative()
{
    if (cg_handler != nullptr) {
        delete cg_handler;
        cg_handler = nullptr;
    }

    if (keyboard_layout_ != nullptr) {
        delete keyboard_layout_;
        keyboard_layout_ = nullptr;
    }

    if (app_kit_ != nullptr)
    {
        delete app_kit_;
        app_kit_ = nullptr;
    }

    instance_ = nullptr;
}

void MacOSNative::RegisterHotKey(uint32_t key, uint32_t modifier)
{
    fprintf(stdout, "Registering Hotkey: key=0x%x, modifier=0x%x\n", key, modifier);
    uint32_t hashRegkey = HashRegKey(key, modifier);

    registered_keys_[hashRegkey] = {
        .key = key,
        .modifier = modifier,
    };
}

void MacOSNative::UnregisterHotKey(uint32_t key, uint32_t modifier)
{
    uint32_t hashRegkey = HashRegKey(key, modifier);
    if (!registered_keys_.contains(hashRegkey)) {
        return;
    }
    registered_keys_.erase(hashRegkey);
}

void MacOSNative::SendKeysDown(uint32_t *keys, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        SendKey(keys[i], true);
    }
}

void MacOSNative::SendKeysUp(uint32_t *keys, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        SendKey(keys[i], false);
    }
}

void MacOSNative::SetMousePos(int x, int y)
{
    /* only focuses on the main display */
    CGDisplayMoveCursorToPoint(CGMainDisplayID(), {
        .x = (float)x,
        .y = (float)y
    });
}

void MacOSNative::GetMousePos(int *x_ret, int *y_ret)
{
    float x;
    float y;
    app_kit_->getCursorPos(&x, &y);
    *x_ret = (int)x;
    *y_ret = (int)y;
}

NativeWindow MacOSNative::GetFocusedWindow()
{
    return static_cast<NativeWindow>(app_kit_->activeProcessId());
}

bool MacOSNative::SetFocusOnWindow(const NativeWindow id)
{
    handle_pending_events();
    return app_kit_->activateProcess(static_cast<pid_t>(id));
}

static int CGWindowLayer(CFDictionaryRef window)
{
    int layer;

    CFNumberRef layerRef = static_cast<CFNumberRef>(CFDictionaryGetValue(window, kCGWindowLayer));
    if (layerRef != nullptr
            && CFNumberGetValue(layerRef, kCFNumberIntType, &layer)) {
        return layer;
    }

    return -1;
}

static std::string CGWindowName(CFDictionaryRef window) {
    constexpr size_t MAX_WINDOW_TITLE_LENGTH = 1024;

    char buffer[MAX_WINDOW_TITLE_LENGTH];
    std::string title;

    CFStringRef titleRef = static_cast<CFStringRef>(CFDictionaryGetValue(window, kCGWindowName));
    if (titleRef != nullptr
            && CFStringGetCString(titleRef, buffer, MAX_WINDOW_TITLE_LENGTH, kCFStringEncodingUTF8)) {
        size_t len = (size_t)CFStringGetLength(titleRef);
        title.append(buffer, len);
    }

    return title;
}

static std::string CGWindowOwnerName(CFDictionaryRef window) {
    constexpr size_t MAX_WINDOW_TITLE_LENGTH = 1024;

    char buffer[MAX_WINDOW_TITLE_LENGTH];
    std::string ownerName;

    CFStringRef ownerNameRef = static_cast<CFStringRef>(CFDictionaryGetValue(window, kCGWindowOwnerName));
    if (ownerNameRef != nullptr
            && CFStringGetCString(ownerNameRef, buffer, MAX_WINDOW_TITLE_LENGTH, kCFStringEncodingUTF8)) {
        size_t len = (size_t)CFStringGetLength(ownerNameRef);
        ownerName.append(buffer, len);
    }

    return ownerName;
}

static int CGWindowPID(CFDictionaryRef window) {
    int pid;

    CFNumberRef layerRef = static_cast<CFNumberRef>(CFDictionaryGetValue(window, kCGWindowOwnerPID));
    if (layerRef != nullptr
            && CFNumberGetValue(layerRef, kCFNumberIntType, &pid)) {
        return pid;
    }

    return -1;
}

bool MacOSNative::IsMainWindowActive(const std::string &window_name)
{
    NativeAppInfo app_info{};
    if (!app_kit_->getForegroundAppInfo(&app_info)) {
        return false;
    }

    if (app_info.name.find(window_name) != std::string::npos) {
        return true;
    }

    // Get window title
    {
        CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
        if (windowList != nullptr) {
            CFIndex count = CFArrayGetCount(windowList);

            for (CFIndex i = 0; i < count; i++) {
                CFDictionaryRef window = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
                if (CGWindowLayer(window) != 0) {
                    continue;
                }

                std::string title = CGWindowName(window);

                if (CGWindowPID(window) == app_info.pid) {
                    app_info.title = std::move(title);
                    break;
                }
            }

            CFRelease(windowList);
        }
    }

    return app_info.title.find(window_name) != std::string::npos;
}

bool MacOSNative::SetFocusOnWindow(const std::string &window_name)
{
    CFArrayRef windowList = CGWindowListCopyWindowInfo(kCGWindowListOptionAll | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (windowList != nullptr) {
        CFIndex count = CFArrayGetCount(windowList);

        for (CFIndex i = 0; i < count; i++) {
            CFDictionaryRef window = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windowList, i));
            if (CGWindowLayer(window) != 0) {
                continue;
            }

            std::string title = CGWindowName(window);
            int pid = -1;
            if (title.find(window_name) != std::string::npos) {
                pid = CGWindowPID(window);
            } else {
                std::string appName = CGWindowOwnerName(window);
                if (appName.find(window_name) != std::string::npos) {
                    pid = CGWindowPID(window);
                }
            }

            if (pid > 0) {
                if (SetFocusOnWindow(static_cast<NativeWindow>(pid))) {
                    return true;
                }
            }
        }

        CFRelease(windowList);
    }
    return false;
}

void MacOSNative::CursorHide(bool hide)
{
    if (hide) {
        CGDisplayHideCursor(CGMainDisplayID());
    } else {
        CGDisplayShowCursor(CGMainDisplayID());
    }
    app_kit_->cursorHide(hide);
}

void MacOSNative::Update()
{
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

bool MacOSNative::HotkeyHandler(uint32_t keycode, uint32_t modifier)
{
    uint32_t hash = HashRegKey(keycode, modifier);
    if (!MacOSNative::instance_->registered_keys_.contains(hash)) {
        return false;
    }
    MacOSNative::instance_->hotkey_events_.push(HotkeyEvent(keycode, modifier));
    return true;
}

bool MacOSNative::MouseButtonHandler(uint32_t key, bool is_down, int x, int y)
{
    MacOSNative::instance_->mouse_button_evnets_.push(MouseButtonEvent(key, is_down, x, y));
    return false;
}

void MacOSNative::SendKey(uint32_t key, bool is_down)
{
    if (keyboard_layout_) {
        if (!keyboard_layout_->send_key(0, key, is_down)) {
            fprintf(stderr, "failed to send key...\n");
        }
    }
}

void MacOSNative::SendChar(char32_t chr, uint32_t key, bool is_down)
{
    if (keyboard_layout_) {
        if (!keyboard_layout_->send_key(chr, 0, is_down)) {
            fprintf(stderr, "failed to send char...\n");
        }
    }
}
