#import "macos_appkit_impl.h"

#import <AppKit/NSWorkspace.h>
#import <AppKit/NSScreen.h>
#import <AppKit/NSCursor.h>

@implementation AppKitImpl

AppKit::AppKit()
{
    self = [[AppKitImpl alloc] init];
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:static_cast<id>(self)
                                                           selector:@selector(didDeactivateApplicationObserver:)
                                                               name:NSWorkspaceDidDeactivateApplicationNotification
                                                             object:nil];
}

AppKit::~AppKit()
{
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:static_cast<id>(self)];
    [static_cast<id>(self) dealloc];
}

//
// Update last active application property
//
- (void) didDeactivateApplicationObserver:(NSNotification *) notification
{
    NSDictionary *userInfo = notification.userInfo;
    NSRunningApplication *app = userInfo[NSWorkspaceApplicationKey];

    if (app.processIdentifier != [self ownProcessId]) {
        self.lastActiveApplication = app;
    }
}

//
// Add global event monitor
//
- (id) addGlobalMonitor:(NSEventMask) mask handler:(void (^)(NSEvent *)) handler
{
    return [NSEvent addGlobalMonitorForEventsMatchingMask:mask handler:handler];
}

//
// Remove global event monitor
//
- (void) removeGlobalMonitor:(id) monitor
{
    [NSEvent removeMonitor:monitor];
}

//
// Check if accessibility is enabled, may show an popup asking for permissions
//
- (bool) enableAccessibility
{
    NSDictionary *opts = @{static_cast<id>(kAXTrustedCheckOptionPrompt): @YES};
    return AXIsProcessTrustedWithOptions(static_cast<CFDictionaryRef>(opts));
}

//
// Get process id of frontmost application (-> keyboard input)
//
- (pid_t) activeProcessId
{
    return [NSWorkspace sharedWorkspace].frontmostApplication.processIdentifier;
}

//
// Get process id of own process
//
- (pid_t) ownProcessId
{
    return [NSProcessInfo processInfo].processIdentifier;
}

//
// Activate application by process id
//
- (bool) activateProcess:(pid_t) pid
{
    AXUIElementRef element = AXUIElementCreateApplication(pid);
    if (element) 
    {
        CFArrayRef array;
        AXUIElementCopyAttributeValues(element, kAXWindowsAttribute, 0, 99999, &array);

        if (array == nullptr)
        {
            return false;
        }

        NSArray *windows = (NSArray *)CFBridgingRelease(array);
        for (NSUInteger i = 0; i < windows.count; ++i) 
        {
            AXUIElementRef ref = (__bridge AXUIElementRef)(windows[i]);
            AXError error = AXUIElementPerformAction(ref, kAXRaiseAction);
            // Can try again: https://developer.apple.com/documentation/applicationservices/1462091-axuielementperformaction#discussion
            if (error != 0 && error != kAXErrorCannotComplete) 
            {
                return false;
            }
        }
        CFRelease(element);
    }
    NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];

    return app && [app activateWithOptions:NSApplicationActivateAllWindows | NSApplicationActivateIgnoringOtherApps];
}

- (void) getCursorPos:(float*) x y: (float*) y
{
    const NSPoint mouseLoc = [NSEvent mouseLocation];
    const NSRect screenRect = [NSScreen mainScreen].frame;
    *x = abs(mouseLoc.x);
    *y = abs(mouseLoc.y - screenRect.size.height);
}

- (void) cursorHide:(bool) hide
{
    if (hide) 
    {
        [NSCursor hide];
    }
    else 
    {
        [NSCursor unhide];
    }
}

- (bool) getForegroundAppInfo:(NativeAppInfo*) result
{
    if (!result) 
    {
        return false;
    }
    NSRunningApplication *rApp = [[NSWorkspace sharedWorkspace] frontmostApplication];
    if (!rApp) 
    {
        return false;
    }
    result->pid = rApp.processIdentifier;
    result->name = rApp.localizedName.UTF8String;
    return true;
}

//
// ------------------------- C++ Trampolines -------------------------
//

void* AppKit::addGlobalMonitor(CGKeyCode keycode, CGEventFlags modifier, void *userData, void (*handler)(void *))
{
    return [static_cast<id>(self) addGlobalMonitor:NSEventMaskKeyDown handler:^(NSEvent *event) {
        if (event.keyCode == keycode && (event.modifierFlags & modifier) == modifier) {
            handler(userData);
        }
    }];
}

void AppKit::removeGlobalMonitor(void *monitor)
{
    return [static_cast<id>(self) removeGlobalMonitor:static_cast<id>(monitor)];
}

bool AppKit::enableAccessibility()
{
    return [static_cast<id>(self) enableAccessibility];
}

pid_t AppKit::lastActiveProcessId()
{
    return [static_cast<id>(self) lastActiveApplication].processIdentifier;
}

pid_t AppKit::activeProcessId()
{
    return [static_cast<id>(self) activeProcessId];
}

pid_t AppKit::ownProcessId()
{
    return [static_cast<id>(self) ownProcessId];
}

bool AppKit::activateProcess(pid_t pid)
{
    return [static_cast<id>(self) activateProcess:pid];
}

void AppKit::getCursorPos(float *x, float *y)
{
    [static_cast<id>(self) getCursorPos:x y:y];
}

void AppKit::cursorHide(bool hide)
{
    [static_cast<id>(self) cursorHide:hide];
}

bool AppKit::getForegroundAppInfo(NativeAppInfo* out)
{
    return [static_cast<id>(self) getForegroundAppInfo:out];
}

@end