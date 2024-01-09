#import "macos_appkit.h"

#import <CoreGraphics/CGEvent.h>
#import <Foundation/Foundation.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSRunningApplication.h>

@interface AppKitImpl : NSObject

@property (strong) NSRunningApplication *lastActiveApplication;

- (bool) enableAccessibility;
- (pid_t) activeProcessId;
- (pid_t) ownProcessId;
- (bool) activateProcess:(pid_t) pid;
- (void) getCursorPos:(float*) x y:(float*) y;
- (void) cursorHide:(bool) hide;
- (bool) getForegroundAppInfo:(NativeAppInfo*) out;

@end