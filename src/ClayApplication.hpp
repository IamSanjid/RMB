#pragma once

#include <cstdint>
#include <string>

#include <thread>
#include <chrono>

// TODO: Move all the implementation to a single .cpp?
#include "views/View.h"
#include "views/ClayMainView.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#define CLAY_IMPLEMENTATION
#define CLAY_RENDERER_GLES3_IMPLEMENTATION
#define CLAY_TEXT_INPUT_IMPLEMENTATION
#define CLAY_BUTTON_IMPLEMENTATION
#define CLAY_DROP_DOWN_IMPLEMENTATION

#include "clay/clay.h"
#include "clay/clay_renderer_gles3.h"
#define _CRT_SECURE_NO_WARNINGS
#include "clay/clay_renderer_gles3_loader_stb.h"
#undef _CRT_SECURE_NO_WARNINGS
#include "clay/clay_text_input.h"
#include "clay/clay_button.h"
#include "clay/clay_drop_down.h"

#include <GLFW/glfw3.h>
#ifdef _MSC_VER
#if _DEBUG
#pragma comment(lib, "build/externals/glfw/debug/windows-x64/glfw3.lib")
#else
#pragma comment(lib, "build/externals/glfw/release/windows-x64/glfw3.lib")
#endif
#endif

#ifdef _DEBUG
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
#endif

static void clay_error_handler(Clay_ErrorData error_data) {
    printf("[Clay Error] %s\n", error_data.errorText.chars);
}

#if GLFW_HAS_WAYLAND
static bool isWayland() {
#if GLFW_HAS_GETPLATFORM
    return glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
#else
    const char* version = glfwGetVersionString();
    if (strstr(version, "Wayland") == nullptr) // e.g. Ubuntu 22.04 ships with GLFW 3.3.6 compiled without Wayland
        return false;
#ifdef GLFW_EXPOSE_NATIVE_X11
    if (glfwGetX11Display() != nullptr)
        return false;
#endif
    return true;
#endif
}
#endif

static float getContentScaleForMonitor(GLFWmonitor* monitor) {
#if GLFW_HAS_WAYLAND
    if (isWayland()) // We can't access our bd->IsWayland cache for a monitor.
        return 1.0f;
#endif
#if GLFW_HAS_PER_MONITOR_DPI && !(defined(__APPLE__) || defined(__EMSCRIPTEN__) || defined(__ANDROID__))
    float x_scale, y_scale;
    glfwGetMonitorContentScale(monitor, &x_scale, &y_scale);
    return x_scale;
#else
    (void)(monitor);
    return 1.0f;
#endif
}

std::chrono::milliseconds operator"" ms(unsigned long long value) {
    return std::chrono::milliseconds(value);
}

// https://github.com/nicbarker/clay/blob/cfee7e8376ae968ba97ea880d56c33b96493dffc/examples/GLES3-GLFW-video-demo/main.c

class ClayApplication {
public:
    ClayApplication() {
        instance_ = this;
    }

    ClayApplication(const ClayApplication&) = delete;
    ClayApplication& operator=(const ClayApplication&) = delete;

    ~ClayApplication() {
        is_running_ = false;

        if (ibeam_cursor_) {
            glfwDestroyCursor(ibeam_cursor_);
        }
        if (main_window_) {
            glfwDestroyWindow(main_window_);
        }
        glfwTerminate();

        main_window_ = nullptr;
        instance_ = nullptr;
        ibeam_cursor_ = nullptr;
    }

    static ClayApplication* GetInstance() {
        return instance_;
    }

    bool Initialize(const char* name, uint32_t width, uint32_t height) {
#ifdef _DEBUG
        glfwSetErrorCallback(glfw_error_callback);
#endif

        if (!glfwInit())
            return false;

        glfwDefaultWindowHints();

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_SAMPLES, 4);

        glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);

        auto main_scale = getContentScaleForMonitor(glfwGetPrimaryMonitor());
        float scaled_width = width * main_scale;
        float scaled_height = height * main_scale;
        main_window_ = glfwCreateWindow((int)scaled_width, (int)scaled_height, name, nullptr, nullptr);
        if (main_window_ == nullptr)
            return false;

        glfwMakeContextCurrent(main_window_);
#if !defined(__APPLE__)
        gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
#endif
        glfwSwapInterval(1);
        glfwSetWindowUserPointer(main_window_, this);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        size_t clay_required_memory = Clay_MinMemorySize();
        gles3_.clayMemory = {
            .capacity = clay_required_memory,
            .memory = (char *)malloc(clay_required_memory),
        };
        Clay_Context *clay_ctx = Clay_Initialize(
            gles3_.clayMemory,
            {
                .width = scaled_width,
                .height = scaled_height,
            },
            {
                .errorHandlerFunction = clay_error_handler
            }
        );

        Clay_SetCurrentContext(clay_ctx);
        Clay_SetMeasureTextFunction(Stb_MeasureText, &stb_fonts_);
        Clay_TextInput_SetPlatform(Clay_TextInput_Platform_GLFW(main_window_));
        Gles3_SetRenderTextFunction(&gles3_, Stb_RenderText, &stb_fonts_);
        Gles3_Initialize(&gles3_, 4096);

        int atlasW = 1024;
        int atlasH = 1024;
        if (!Stb_LoadFont(
                &gles3_.fontTextures[FONT_ID_ROBOTO_REGULAR],
                &stb_fonts_[FONT_ID_ROBOTO_REGULAR],
                "resources/Roboto-Regular.ttf",
                24.0f, // bake pixel height
                atlasW,
                atlasH))
            return false;
        if (!Stb_LoadFont(
                &gles3_.fontTextures[FONT_ID_INTER_REGULAR],
                &stb_fonts_[FONT_ID_INTER_REGULAR],
                "resources/Inter-Wght400.ttf",
                32.0f, // bake pixel height
                atlasW,
                atlasH))
            return false;
        if (!Stb_LoadFont(
                &gles3_.fontTextures[FONT_ID_INTER_MEDIUM],
                &stb_fonts_[FONT_ID_INTER_MEDIUM],
                "resources/Inter-Wght500.ttf",
                32.0f, // bake pixel height
                atlasW,
                atlasH))
            return false;
        if (!Stb_LoadFont(
                &gles3_.fontTextures[FONT_ID_INTER_BOLD],
                &stb_fonts_[FONT_ID_INTER_BOLD],
                "resources/Inter-Wght700.ttf",
                32.0f, // bake pixel height
                atlasW,
                atlasH))
            return false;
        if (!Stb_LoadImage(
                &gles3_.imageTextures[IMAGE_ID_ICON_PRIMARY_KEYBOARD],
                "resources/icons/primary_keyboard.png"))
            return false;
        if (!Stb_LoadImage(
                &gles3_.imageTextures[IMAGE_ID_ICON_LABEL_WINDOW],
                "resources/icons/label_window.png"))
            return false;
        if (!Stb_LoadImage(
                &gles3_.imageTextures[IMAGE_ID_ICON_HEADLINE_HIDE_SOURCE],
                "resources/icons/headline_hide_source.png"))
            return false;
        if (!Stb_LoadImage(
                &gles3_.imageTextures[IMAGE_ID_ICON_HEADLINE_STRAIGHTEN],
                "resources/icons/headline_straighten.png"))
            return false;
#ifdef _DEBUG
        Clay_SetDebugModeEnabled(true);
#endif // _DEBUG
        glfwSetScrollCallback(main_window_, clay_scroll_callback);
        glfwSetKeyCallback(main_window_, on_key_callback);
        glfwSetCharCallback(main_window_, on_char_callback);
        glfwSetFramebufferSizeCallback(main_window_, on_frmaebuffer_size_update);

        views_.push_back(new ClayMainView());

        return true;
    }

    void Run() {
        if (is_running_)
            return;

        is_running_ = true;

        Clay_Vector2 scroll_delta = {0.0f, 0.0f};
        while (!glfwWindowShouldClose(main_window_)) {
            glfwWaitEvents();
            if (glfwGetWindowAttrib(main_window_, GLFW_ICONIFIED)) {
                std::this_thread::sleep_for(10ms);
                continue;
            }

            scroll_delta = scroll_delta_;
            scroll_delta_.x = scroll_delta_.y = 0.f;

            double now = glfwGetTime(); // seconds
            delta_time_ = (now - last_time_) * 1000.0;
            last_time_ = now;

            double mouse_x = 0.0;
            double mouse_y = 0.0;
            glfwGetCursorPos(main_window_, &mouse_x, &mouse_y);

            Clay_Vector2 mouse_position = {
                (float)mouse_x,
                (float)mouse_y
            };
            
            Clay_SetPointerState(mouse_position, glfwGetMouseButton(main_window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
            Clay_UpdateScrollContainers(true, {scroll_delta.x, scroll_delta.y}, (float)delta_time_);
            // Clay_TextInput_Update(delta_time_);

            // int window_w, window_h;
            // glfwGetWindowSize(main_window_, &window_w, &window_h);
            // glViewport(0, 0, window_w, window_h);

            // Clay_SetLayoutDimensions({(float)window_w, (float)window_h});

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE); // Clay renderer is simple and never writes to depth buffer
            glClearColor(0.1f, 0.2f, 0.1f, 1.0f);

            auto current = CurrentView();
            if (current != nullptr) {
                auto result = current->Update();
                switch (result.action) {
                case ViewActionType::NavigateTo:
                    views_.push_back(result.view);
                    break;
                case ViewActionType::NavigateBack:
                    delete current;
                    views_.pop_back();
                    break;
                case ViewActionType::Render:
                    Gles3_Render(&gles3_, result.commands, stb_fonts_);
                    break;
                case ViewActionType::None:
                    break;
                default:
                    printf("Unhandled View update action!\n");
                    std::abort();
                    break;
                }
            }

            glfwSwapBuffers(main_window_);
        }
    }

private:
    static ClayApplication* instance_;

    static void clay_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
        auto thiz = static_cast<ClayApplication*>(glfwGetWindowUserPointer(window));
        thiz->scroll_delta_.x += (float)xoffset;
        thiz->scroll_delta_.y += (float)yoffset;
    }

    static const char *Clay_TextInput__GLFW_GetClipboard(void *ud) {
        return glfwGetClipboardString((GLFWwindow *)ud);
    }

    static void Clay_TextInput__GLFW_SetClipboard(void *ud, const char *text) {
        glfwSetClipboardString((GLFWwindow *)ud, text);
    }

    static GLFWcursor* ibeam_cursor_;
    static void Clay_TextInpupt__GLFW_SetIbeamCursor(void *ud) {
        if (ibeam_cursor_ == NULL) {
            ibeam_cursor_ = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
        }
        glfwSetCursor((GLFWwindow *)ud, ibeam_cursor_);
    }

    static void Clay_TextInput__GLFW_ResetCursor(void *ud) {
        glfwSetCursor((GLFWwindow *)ud, NULL);
    }

    static inline Clay_TextInput_Platform Clay_TextInput_Platform_GLFW(GLFWwindow *window) {
        return CLAY__INIT(Clay_TextInput_Platform){
            .getClipboardText = Clay_TextInput__GLFW_GetClipboard,
            .setClipboardText = Clay_TextInput__GLFW_SetClipboard,
            .setIbeamCursor   = Clay_TextInpupt__GLFW_SetIbeamCursor,
            .resetCursor      = Clay_TextInput__GLFW_ResetCursor,
            .userData         = (void *)window,
        };
    }

    static inline Clay_TI_Action Clay_TextInput__GLFW_TranslateAction(int action) {
        switch (action) {
        case GLFW_RELEASE:  return CLAY_TI_ACTION_RELEASE;
        case GLFW_PRESS:    return CLAY_TI_ACTION_PRESS;
        case GLFW_REPEAT:   return CLAY_TI_ACTION_REPEAT;
        }
    }

    static inline Clay_TI_Key Clay_TextInput__GLFW_TranslateKey(int k) {
        switch (k) {
        case GLFW_KEY_LEFT:      return CLAY_TI_KEY_LEFT;
        case GLFW_KEY_RIGHT:     return CLAY_TI_KEY_RIGHT;
        case GLFW_KEY_HOME:      return CLAY_TI_KEY_HOME;
        case GLFW_KEY_END:       return CLAY_TI_KEY_END;
        case GLFW_KEY_BACKSPACE: return CLAY_TI_KEY_BACKSPACE;
        case GLFW_KEY_DELETE:    return CLAY_TI_KEY_DELETE;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:  return CLAY_TI_KEY_ENTER;
        case GLFW_KEY_ESCAPE:    return CLAY_TI_KEY_ESCAPE;
        case GLFW_KEY_A:         return CLAY_TI_KEY_A;
        case GLFW_KEY_C:         return CLAY_TI_KEY_C;
        case GLFW_KEY_V:         return CLAY_TI_KEY_V;
        case GLFW_KEY_X:         return CLAY_TI_KEY_X;
        default:                 return CLAY_TI_KEY_UNKNOWN;
        }
    }
    
    static inline Clay_TI_Mod Clay_TextInput__GLFW_TranslateMods(int m) {
        Clay_TI_Mod r = CLAY_TI_MOD_NONE;
        if (m & GLFW_MOD_SHIFT)   r = (Clay_TI_Mod)(r | CLAY_TI_MOD_SHIFT);
        if (m & GLFW_MOD_CONTROL) r = (Clay_TI_Mod)(r | CLAY_TI_MOD_CTRL);
        if (m & GLFW_MOD_ALT)     r = (Clay_TI_Mod)(r | CLAY_TI_MOD_ALT);
        if (m & GLFW_MOD_SUPER)   r = (Clay_TI_Mod)(r | CLAY_TI_MOD_SUPER);
        return r;
    }

    static void on_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto thiz = static_cast<ClayApplication*>(glfwGetWindowUserPointer(window));
        
        auto current = thiz->CurrentView();
        if (current == nullptr)
            return;

        switch (action) {
        case GLFW_PRESS:
            current->OnKeyPress(key, scancode, mods);
            break;
        case GLFW_RELEASE:
            current->OnKeyRelease(key, scancode, mods);
            break;
        default:
            break;
        }

        Clay_TextInput_OnKey(Clay_TextInput__GLFW_TranslateKey(key),
                             Clay_TextInput__GLFW_TranslateAction(action),
                             Clay_TextInput__GLFW_TranslateMods(mods));
    }

    static void on_char_callback(GLFWwindow* window, unsigned int cp) {
        (void)window;
        Clay_TextInput_OnChar(cp);
    }

    static void on_frmaebuffer_size_update(GLFWwindow* window, int width, int height) {
        int window_w, window_h;
        glfwGetWindowSize(window, &window_w, &window_h);
        Clay_SetLayoutDimensions({(float)window_w, (float)window_h});

        glViewport(0, 0, width, height);
    }

    View* CurrentView() {
        return views_.empty() ? nullptr : views_.back();
    }

    std::vector<View*> views_{};

    bool is_running_ = false;

    GLFWwindow* main_window_ = nullptr;
    Clay_Vector2 scroll_delta_ = {0.0f, 0.0f};
    double last_time_ = 0.0;
    double delta_time_ = 0.0;
    Stb_FontData stb_fonts_[MAX_FONTS]; // Fonts userData
    Gles3_Renderer gles3_;              // The renderer itself
};

ClayApplication* ClayApplication::instance_ = nullptr;
GLFWcursor* ClayApplication::ibeam_cursor_ = nullptr;
