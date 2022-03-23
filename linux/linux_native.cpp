#include "linux_native.h"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/XInput2.h>

#include <thread>
#include <functional>
#include "Utils.h"

Native* Native::GetInstance() 
{
	return LinuxNative::GetInstance();
}

LinuxNative* LinuxNative::GetInstance()
{
	if (!instance_)
		instance_ = new LinuxNative();
	return instance_;
}

LinuxNative* LinuxNative::instance_ = nullptr;

LinuxNative::LinuxNative()
{
    display_name_ = XDisplayName(display_name_);
    display_ = XOpenDisplay(display_name_);

    if (!display_)
        return;

    XSelectInput(display_, DefaultRootWindow(display_), KeyPressMask);

    /* getting scan code infos */

    int keycode_low, keycode_high, keysyms_per_keycode;

    XDisplayKeycodes(display_, &keycode_low, &keycode_high);
    KeySym *keysyms = XGetKeyboardMapping(display_, keycode_low,
                                    keycode_high - keycode_low + 1,
                                    &keysyms_per_keycode);
    XFree(keysyms);

    /*uint32_t keycodes_length = ((keycode_high - keycode_low) + 1)
                        * keysyms_per_keycode;*/

    XkbDescPtr desc = XkbGetMap(display_, XkbAllClientInfoMask, XkbUseCoreKbd);

    for (int keycode = keycode_low; keycode <= keycode_high; keycode++) 
    {
        uint32_t groups = XkbKeyNumGroups(desc, keycode);
        for (uint32_t group = 0; group < groups; group++) 
        {
            XkbKeyTypePtr key_type = XkbKeyKeyType(desc, keycode, group);
            for (uint32_t level = 0; level < key_type->num_levels; level++) 
            {
                KeySym keysym = XkbKeycodeToKeysym(display_, keycode, group, level);
                uint32_t modmask = 0;

                for (int num_map = 0; num_map < key_type->map_count; num_map++) 
                {
                    XkbKTMapEntryRec map = key_type->map[num_map];
                    if (map.active && map.level == level) 
                    {
                        modmask = map.mods.mask;
                        break;
                    }
                }
                /* keep one version without any modifier mask */
                auto exists = scan_code_infos_.find(keycode);
                if (exists != scan_code_infos_.cend())
                {
                    if (exists->second.modmask && !modmask)
                    {
                        scan_code_infos_[keycode] = { group, modmask | KeyCodeToModifier(keycode), keysym };
                    }
                }
                else
                {
                    scan_code_infos_[keycode] = { group, modmask | KeyCodeToModifier(keycode), keysym };
                }
            }
        }
    }

    XkbFreeClientMap(desc, 0, 1);
}

LinuxNative::~LinuxNative()
{
    if (display_)
    {
        XCloseDisplay(display_);
    }
    for (auto& it : registered_keys_)
    {
        UnregisterHotKey(it.second.key, it.second.modifier);
    }
	instance_ = nullptr;
}

void LinuxNative::Update()
{
    while (XPending(display_))
    {
        XEvent event;
        XNextEvent(display_, &event);

        switch (event.type)
        {
            case KeyPress:
            {
                const auto& key_data = event.xkey;
                uint32_t hash = HashRegKey(key_data.keycode, key_data.state & ~(LockMask | Mod2Mask));
                auto matched_reg_key_it = registered_keys_.find(hash);
                if (matched_reg_key_it != registered_keys_.cend())
                {
                    const auto& matched_reg_key = matched_reg_key_it->second;
                    EventBus::Instance().publish(new HotkeyEvent(matched_reg_key.key, matched_reg_key.modifier));
                }
                else
                {
                    hash = HashRegKey(key_data.keycode, AnyModifier);
                    matched_reg_key_it = registered_keys_.find(hash);
                    if (matched_reg_key_it != registered_keys_.cend())
                    {
                        const auto& matched_reg_key = matched_reg_key_it->second;
                        EventBus::Instance().publish(new HotkeyEvent(matched_reg_key.key, matched_reg_key.modifier));
                    }
                }
                break;
            }
        }
    }
}

void LinuxNative::RegisterHotKey(uint32_t key, uint32_t modifier)
{
    if (scan_code_infos_.find(key) == scan_code_infos_.cend()) 
        return;
    
    uint32_t modmask = AnyModifier;
    if (modifier != 0)
    {
        auto found = scan_code_infos_.find(modifier);
        if (found == scan_code_infos_.cend())
            return;
        modmask = found->second.modmask;
    }

    uint32_t hash = HashRegKey(key, modmask);
    if (registered_keys_.find(hash) != registered_keys_.cend())
        return;
    
    for (int screen = 0; screen < ScreenCount(display_); screen++)
    {
        Window root_window = RootWindow(display_, screen);

        int error = XGrabKey(display_, key, modmask, root_window, False, GrabModeAsync, GrabModeAsync);
        error &= XGrabKey(display_, key, modmask | LockMask, root_window, False, GrabModeAsync, GrabModeAsync);
        error &= XGrabKey(display_, key, modmask | Mod2Mask, root_window, False, GrabModeAsync, GrabModeAsync);
        error &= XGrabKey(display_, key, modmask | LockMask | Mod2Mask, root_window, False, GrabModeAsync, GrabModeAsync);
        if (error <= 1)
        {
            fprintf(stdout, "Registered hot key: %d, %d\n", (int)key, (int)modifier);
            registered_keys_[hash] = { key, modifier };
        }
    }
    XSync(display_, False);
    XFlush(display_);
}

void LinuxNative::UnregisterHotKey(uint32_t key, uint32_t modifier)
{
    if (scan_code_infos_.find(key) == scan_code_infos_.cend()) 
        return;

    uint32_t modmask = AnyModifier;
    if (modifier != 0)
    {
        auto found = scan_code_infos_.find(modifier);
        if (found == scan_code_infos_.cend())
            return;
        modmask = found->second.modmask;
    }

    uint32_t hash = HashRegKey(key, modmask);
    if (registered_keys_.find(hash) == registered_keys_.cend())
    {
        return;
    }

    for (int screen = 0; screen < ScreenCount(display_); screen++)
    {
        Window root_window = RootWindow(display_, screen);

        XUngrabKey(display_, key, modmask, root_window);
        XUngrabKey(display_, key, modmask | LockMask, root_window);
        XUngrabKey(display_, key, modmask | Mod2Mask, root_window);
        XUngrabKey(display_, key, modmask | LockMask | Mod2Mask, root_window);
        registered_keys_.erase(hash);
    }
    XSync(display_, False);
    XFlush(display_);
}

void LinuxNative::SendKeysDown(uint32_t* keys, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        SendKey(keys[i], true);
    }
    XFlush(display_);
}

void LinuxNative::SendKeysUp(uint32_t* keys, size_t count)
{
	for (size_t i = 0; i < count; i++)
    {
        SendKey(keys[i], false);
    }
    XFlush(display_);
}

void LinuxNative::SetMousePos(double x, double y)
{
    int screen = -1;

    GetDefaultScreenMousePos(NULL, NULL, &screen, NULL);
    XWarpPointer(display_, None, RootWindow(display_, screen), 0, 0, 0, 0, (int)x, (int)y);
    XFlush(display_);
}

static int IgnoreBadWindow(Display *dpy, XErrorEvent *xerr) 
{
    if (xerr->error_code == BadWindow)
        return 0;
    /* TODO: quite the program after reporting the error... */
    return xerr->error_code;
}

bool LinuxNative::SetFocusOnProcess(const std::string& process_name)
{
    struct EnumWind
    {
        LinuxNative* sender_;
        const char* target_proc_name;
        bool result = false;
    };
    EnumWind enum_wind{ this, process_name.c_str() };

    EnumAllWindow([](Window window, void* ptr)
    {
        EnumWind* ew = (EnumWind*)ptr;
        static auto atom_window_type_id = XInternAtom(ew->sender_->display_, "_NET_WM_WINDOW_TYPE", True);
        static auto atom_target_type = XInternAtom(ew->sender_->display_, "_NET_WM_WINDOW_TYPE_NORMAL", True);

        XWindowAttributes attr;
        XClassHint classhint;
        XGetWindowAttributes(ew->sender_->display_, window, &attr);

        if (attr.map_state != IsViewable)
            return;

        if (XGetClassHint(ew->sender_->display_, window, &classhint)) 
        {
            if (classhint.res_name && strstr(classhint.res_name, ew->target_proc_name))
            {
                XFree(classhint.res_name);
                XFree(classhint.res_class);

                Atom* atom_window_type = (Atom*)ew->sender_->GetWindowPropertyByAtom(window, atom_window_type_id);
                if (!atom_window_type || atom_target_type != *atom_window_type)
                {
                    if (atom_window_type)
                        free(atom_window_type);
                    return;
                }
                free(atom_window_type);

                ew->result = ew->sender_->ActivateWindow(window);
                return;
            }
            XFree(classhint.res_name);
            XFree(classhint.res_class);
        }
    }, (void*)&enum_wind);
    
	return enum_wind.result;
}

void LinuxNative::CursorHide(bool hide)
{
    static bool is_hidden = false;
    const int screencount = ScreenCount(display_);
    if (hide && !is_hidden)
    {
        for (int screen = 0; screen < screencount; screen++) 
        {
            auto root_window = RootWindow(display_, screen);
            /* Create a transparent cursor 
            char bm[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
            Pixmap pix = XCreateBitmapFromData(display_, root_window, bm, 8, 8);
            XColor black;

            memset(&black, 0, sizeof(XColor));
            black.flags = DoRed | DoGreen | DoBlue;
            auto cursor = XCreatePixmapCursor(display_, pix, pix, &black, &black, 0, 0);
            XFreePixmap(display_, pix);

            XDefineCursor(display_, root_window, cursor);
            XGrabPointer(display_, root_window, True, 0, GrabModeAsync, GrabModeAsync, root_window, cursor, CurrentTime);*/
            XFixesHideCursor(display_, root_window);
        }
        is_hidden = true;
    }
    else if (is_hidden)
    {
        for (int screen = 0; screen < screencount; screen++) 
        {
            auto root_window = RootWindow(display_, screen);
            /*XUndefineCursor(display_, root_window);
            XUngrabPointer(display_, CurrentTime);*/
            XFixesShowCursor(display_, root_window);
        }
        is_hidden = false;
    }

    XSync(display_, False);
    XFlush(display_);
}

void LinuxNative::GetMousePos(double* x_ret, double* y_ret)
{
    int def_x, def_y;
    if (GetDefaultScreenMousePos(&def_x, &def_y, NULL, NULL))
    {
        *x_ret = static_cast<double>(def_x);
        *y_ret = static_cast<double>(def_y);
    }
    else
    {
        *x_ret = 0.0;
        *y_ret = 0.0;
    }
}

bool LinuxNative::GetDefaultScreenMousePos(int* x_ret, int* y_ret, int* screen_ret, Window* window_ret)
{
    int ret = false;
    int x = 0, y = 0, screen_num = 0;
    int i = 0;
    Window window = 0;
    Window root = 0;
    int dummy_int = 0;
    unsigned int dummy_uint = 0;
    int screencount = ScreenCount(display_);

    for (i = 0; i < screencount; i++) 
    {
        Screen *screen = ScreenOfDisplay(display_, i);
        ret = XQueryPointer(display_, RootWindowOfScreen(screen),
                        &root, &window,
                        &x, &y, &dummy_int, &dummy_int, &dummy_uint);
        if (ret == true) 
        {
            screen_num = i;
            break;
        }
    }
    if (ret == true)
    {
        if (x_ret != NULL) *x_ret = x;
        if (y_ret != NULL) *y_ret = y;
        if (screen_ret != NULL) *screen_ret = screen_num;
        if (window_ret != NULL) *window_ret = window;
        return true;
    }
    return false;
}

unsigned char* LinuxNative::GetWindowPropertyByAtom(Window window, Atom atom, long *nitems, Atom *type, int *size)
{
    Atom actual_type;
    int actual_format;
    unsigned long _nitems;
    unsigned long bytes_after;
    unsigned char *prop;
    int status;

    status = XGetWindowProperty(display_, window, atom, 0, (~0L),
                            False, AnyPropertyType, &actual_type,
                            &actual_format, &_nitems, &bytes_after,
                            &prop);
    if (status == BadWindow) 
    {
        fprintf(stderr, "window id # 0x%lx does not exists!", window);
        return NULL;
    } 
    if (status != Success) 
    {
        fprintf(stderr, "XGetWindowProperty failed!");
        return NULL;
    }

    if (nitems != NULL) 
    {
        *nitems = _nitems;
    }

    if (type != NULL) 
    {
        *type = actual_type;
    }

    if (size != NULL)
    {
        *size = actual_format;
    }
    return prop;
}

uint32_t LinuxNative::HashRegKey(int key, uint32_t modmask)
{
    return key * 0x7FFFu + modmask;
}

uint32_t LinuxNative::KeyCodeToModifier(KeyCode keycode)
{
    XModifierKeymap *modmap = XGetModifierMapping(display_);
    uint32_t res = 0;
    for (int i = 0; i < 8; i++) 
    {
        for (int j = 0; j < modmap->max_keypermod && modmap->modifiermap[(i * modmap->max_keypermod) + j]; j++) 
        {
            if (keycode == modmap->modifiermap[(i * modmap->max_keypermod) + j]) 
            {
                switch (i) 
                {
                    case ShiftMapIndex: res = ShiftMask; goto RET;
                    case LockMapIndex: res =  LockMask; goto RET;
                    case ControlMapIndex: res =  ControlMask; goto RET;
                    case Mod1MapIndex: res =  Mod1Mask; goto RET;
                    case Mod2MapIndex: res =  Mod2Mask; goto RET;
                    case Mod3MapIndex: res =  Mod3Mask; goto RET;
                    case Mod4MapIndex: res =  Mod4Mask; goto RET;
                    case Mod5MapIndex: res =  Mod5Mask; goto RET;
                }
            }
        }
    }
    RET:
    XFreeModifiermap(modmap);
    return res;
}

void LinuxNative::SendKey(int key, bool is_down)
{
    XkbStateRec state;
    if (scan_code_infos_.find(key) == scan_code_infos_.cend())
        return;
    
    const auto& scan_code_info = scan_code_infos_[key];

    XkbGetState(display_, XkbUseCoreKbd, &state);
    int current_group = state.group;
    XkbLockGroup(display_, XkbUseCoreKbd, scan_code_info.group);

    auto key_modifier = KeyCodeToModifier(key);
    if (key_modifier)
    {
        SendModifier(key_modifier, is_down);
        XFlush(display_);
        return;
    }
    
    XTestFakeKeyEvent(display_, key, is_down, CurrentTime);
    XkbLockGroup(display_, XkbUseCoreKbd, current_group);
    XSync(display_, False);
    XFlush(display_);
}

void LinuxNative::SendModifier(int modmask, int is_press)
{
    XModifierKeymap *modifiers = XGetModifierMapping(display_);
    for (auto mod_index = ShiftMapIndex; mod_index <= Mod5MapIndex; mod_index++) 
    {
        if (modmask & (1 << mod_index)) 
        {
            for (auto mod_key = 0; mod_key < modifiers->max_keypermod; mod_key++)
            {
                auto keycode = modifiers->modifiermap[mod_index * modifiers->max_keypermod + mod_key];
                if (keycode)
                {
                    XTestFakeKeyEvent(display_, keycode, is_press, CurrentTime);
                    XSync(display_, False);
                    break;
                }
            }
        }
    }
    XFreeModifiermap(modifiers);
}

bool LinuxNative::ActivateWindow(Window window)
{
    auto EWMHIsSupported = [&](const char* feature)
    {
        long nitems = 0L;
        Atom* results = NULL;
        long i = 0;

        Window root;
        Atom request;
        Atom feature_atom;

        request = XInternAtom(display_, "_NET_SUPPORTED", False);
        feature_atom = XInternAtom(display_, feature, False);
        root = XDefaultRootWindow(display_);

        results = (Atom*)GetWindowPropertyByAtom(root, request, &nitems);
        for (i = 0L; i < nitems; i++) 
        {
            if (results[i] == feature_atom) 
            {
                free(results);
                return True;
            }
        }
        free(results);

        return False;
    };

    if (EWMHIsSupported("_NET_ACTIVE_WINDOW") == False) 
    {
        return False;
    }

    if (EWMHIsSupported("_NET_WM_DESKTOP") == True
        && EWMHIsSupported("_NET_CURRENT_DESKTOP") == True) 
    {
        long nitems;
        auto request = XInternAtom(display_, "_NET_WM_DESKTOP", False);
        auto data = GetWindowPropertyByAtom(window, request, &nitems, NULL, NULL);

        if (nitems > 0)
        {
            auto root = RootWindow(display_, 0);
            long desktop = *(long*)data;
            XEvent xev = {};

            xev.type = ClientMessage;
            xev.xclient.display = display_;
            xev.xclient.window = root;
            xev.xclient.message_type = XInternAtom(display_, "_NET_CURRENT_DESKTOP", False);
            xev.xclient.format = 32;
            xev.xclient.data.l[0] = desktop;
            xev.xclient.data.l[1] = CurrentTime;

            XSendEvent(display_, root, False,
                        SubstructureNotifyMask | SubstructureRedirectMask,
                        &xev);
        }
    }

    XEvent xev = {};
    XWindowAttributes wattr;

    xev.type = ClientMessage;
    xev.xclient.display = display_;
    xev.xclient.window = window;
    xev.xclient.message_type = XInternAtom(display_, "_NET_ACTIVE_WINDOW", False);
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 2L;
    xev.xclient.data.l[1] = CurrentTime;

    XGetWindowAttributes(display_, window, &wattr);
    int ret = XSendEvent(display_, wattr.screen->root, False,
                    SubstructureNotifyMask | SubstructureRedirectMask,
                    &xev);
    return ret != BadWindow && ret != BadValue;
}

void LinuxNative::EnumAllWindow(EnumWindowProc enumWinProc, void* userDefinedPtr)
{
    std::function<void(Window)> IterateChildWindows;

    IterateChildWindows = [&](Window window)
    {
        Window dummy;
        Window *children = NULL;
        unsigned int i, nchildren;
        Status success = XQueryTree(display_, window, &dummy, &dummy, &children, &nchildren);
        if (!success) 
        {
            if (children != NULL)
                XFree(children);
            return;
        }
        for (i = 0; i < nchildren; i++) 
        {
            enumWinProc(children[i], userDefinedPtr);
        }

        for (i = 0; i < nchildren; i++) 
        {
            IterateChildWindows(children[i]);
        }

        if (children != NULL)
            XFree(children);
    };

    auto old_error_handler = XSetErrorHandler(IgnoreBadWindow);

	const int screencount = ScreenCount(display_);
    for (int i = 0; i < screencount; i++) 
    {
        Window root = RootWindow(display_, i);
        enumWinProc(root, userDefinedPtr);
        IterateChildWindows(root);
    }

    XSetErrorHandler(old_error_handler);
}