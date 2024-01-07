#ifndef MAC_NATIVE_H
#define MAC_NATIVE_H

#include <CoreGraphics/CGEvent.h>
#include <unistd.h>
#include <string>

extern "C" {

struct NativeAppInfo {
    pid_t pid;
    std::string name;
    std::string title;
};

class AppKit
{
public:
    AppKit();
    ~AppKit();

    void *addGlobalMonitor(CGKeyCode keycode, CGEventFlags modifier, void *userData, void (*handler)(void *));
    void removeGlobalMonitor(void *monitor);
    bool enableAccessibility();
    pid_t lastActiveProcessId();
    pid_t activeProcessId();
    pid_t ownProcessId();
    bool activateProcess(pid_t pid);
    void getCursorPos(float* x, float* y);
    void cursorHide(bool);
    bool getForegroundAppInfo(NativeAppInfo*);

private:
    void *self;
};

}  // extern "C"

#endif  // MAC_NATIVE_H