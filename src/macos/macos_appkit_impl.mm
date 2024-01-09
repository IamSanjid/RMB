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

//
// Gets current cursor coordinates on the screen where the (0, 0) is at the top left corner of the screen
//
- (void) getCursorPos:(float*) x y: (float*) y
{
    const NSPoint mouseLoc = [NSEvent mouseLocation];
    const NSRect screenRect = [NSScreen mainScreen].frame;
    *x = abs(mouseLoc.x);
    *y = abs(mouseLoc.y - screenRect.size.height);
}

//
// Hides/Unhides the cursor from the screen
//
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

//
// Gets the name and the process id of the current front most(basically current active) application
//
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
    if (rApp.localizedName.length > 0) {
        result->name = strdup(rApp.localizedName.UTF8String);
        /*int len = rApp.localizedName.count + 1;
        result->name = malloc(len * sizeof(CChar));
        result->name[len] = '\0';
        memcpy(result->name, rApp.localizedName.UTF8String, len);*/
    }
    return true;
}

//
// ------------------------- C++ Trampolines -------------------------
//

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