#ifndef MAC_NATIVE_H
#define MAC_NATIVE_H

#include <string.h>
#include <unistd.h>

extern "C" {

struct NativeAppInfo {
    pid_t pid;
    const char* name;
};

class AppKit {
public:
    AppKit();
    ~AppKit();

    bool enableAccessibility();
    pid_t lastActiveProcessId();
    pid_t activeProcessId();
    pid_t ownProcessId();
    bool activateProcess(pid_t pid);
    void getCursorPos(float* x, float* y);
    void cursorHide(bool);
    bool getForegroundAppInfo(NativeAppInfo*);

    void requestPermissions();
    bool isMainWindowActive(const char* window_name);
    bool setFocusOnWindowId(int id);
    bool setFocusOnWindow(const char* window_name);

private:
    void* self;
};

typedef bool (*HotkeyHandlerCallback)(uint32_t, uint32_t);
typedef bool (*MouseButtonHandlerCallback)(uint32_t, bool, int, int);
struct CGeventHandlerCallbacks {
    HotkeyHandlerCallback hotkeyHandler;
    MouseButtonHandlerCallback mouseButtonHandler;
};

struct CGEventHandlerInner;
struct CGEventHandler {
    CGeventHandlerCallbacks cachedCallbacks;
    CGEventHandlerInner* inner;

    CGEventHandler(HotkeyHandlerCallback hkHandler, MouseButtonHandlerCallback mbHandler);
    ~CGEventHandler();

    void Update();
    bool SendKey(char32_t chr, uint32_t code, bool down);
    void SetMousePos(int x, int y);
    void CursorHide(bool hide);
};

} // extern "C"

#endif // MAC_NATIVE_H