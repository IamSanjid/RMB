#include "Application.h"
#include "native.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include <GLFW/glfw3.h>
#ifdef _MSC_VER
#if _DEBUG
#pragma comment(lib, "build/externals/glfw/debug/windows-x64/glfw3.lib")
#else
#pragma comment(lib, "build/externals/glfw/release/windows-x64/glfw3.lib")
#endif
#endif

#include "Config.h"
#include "Utils.h"
#include "mouse.h"
#include "keyboard_manager.h"
#include "npad_controller.h"
#include "views/MainView.h"

#include <thread>
#include <stdio.h>

static int screen_center_x_ = 0;
static int screen_center_y_ = 0;

std::unique_ptr<Application> Application::Create() {
    auto app = std::make_unique<Application>();
    if (app->Initialize(Config::Current()->NAME, Config::Current()->WIDTH,
                        Config::Current()->HEIGHT)) {
        return app;
    }
    return nullptr;
}

Application* Application::GetInstance() {
    return instance_;
}

double Application::GetTotalRunningTime() {
    using Time = std::chrono::high_resolution_clock;
    using fmsec = std::chrono::duration<double, std::milli>;

    static auto starting_time = Time::now().time_since_epoch();

    auto current_time = Time::now().time_since_epoch();

    return std::chrono::duration_cast<fmsec>(current_time - starting_time).count();
}

Application* Application::instance_ = nullptr;

Application::Application() {
    instance_ = this;
    GetTotalRunningTime();
}

Application::~Application() {
    is_running_ = false;
    panning_started_ = false;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(main_window_);
    glfwTerminate();

    delete controller_;
    delete mouse_;
    delete main_view_;

    main_window_ = nullptr;
    controller_ = nullptr;
    mouse_ = nullptr;
    main_view_ = nullptr;

    instance_ = nullptr;
}

#ifdef _DEBUG
static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
#endif

bool Application::Initialize(const char* name, uint32_t width, uint32_t height) {
#ifdef _DEBUG
    glfwSetErrorCallback(glfw_error_callback);
#endif
    if (!glfwInit())
        return false;

    EventBus::Instance().subscribe(&Application::OnHotkey);
    EventBus::Instance().subscribe(&Application::OnMouseButton);

#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    main_window_ = glfwCreateWindow(width, height, name, NULL, NULL);
    if (main_window_ == NULL)
        return false;

    controller_ = new NpadController();
    mouse_ = new Mouse();

    glfwMakeContextCurrent(main_window_);
    glfwSwapInterval(1);
    glfwSetKeyCallback(main_window_, OnKeyCallback);

    Reconfig();
#if _DEBUG
    Native::GetInstance()->RegisterHotKey(glfwGetKeyScancode(GLFW_KEY_T),
                                          glfwGetKeyScancode(GLFW_KEY_LEFT_CONTROL));
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    main_view_ = new MainView();
    return ImGui_ImplGlfw_InitForOpenGL(main_window_, true) && ImGui_ImplOpenGL3_Init(glsl_version);
}

void Application::Run() {
    if (is_running_)
        return;

    is_running_ = true;

    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.IniFilename = NULL;
    ImGui::StyleColorsDark();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    /* worker thread */
    auto update_thread = std::jthread{[this](std::stop_token stop_token) {
        while (!stop_token.stop_requested()) {
            Update();
        }
        fprintf(stdout, "Exiting Application.\n");
    }};

    while (!glfwWindowShouldClose(main_window_)) {
        glfwWaitEvents();
        if (glfwGetWindowAttrib(main_window_, GLFW_ICONIFIED)) {
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        main_view_->Show();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(main_window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(main_window_);
    }
}

void Application::Update() {
    Native::GetInstance()->Update();
    DetectMouseMove();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void Application::Reconfig(Config* new_conf) {
    /* getting scan codes for current platform */
    Native::GetInstance()->UnregisterHotKey(Config::Current()->TOGGLE_KEY,
                                            Config::Current()->TOGGLE_MODIFIER);
    if (new_conf)
        Config::Current(new_conf);
    else
        Config::Current(Config::Default());

    auto current_config = Config::Current();

    current_config->TOGGLE_KEY = glfwGetKeyScancode(current_config->TOGGLE_KEY);
    current_config->TOGGLE_MODIFIER = glfwGetKeyScancode(current_config->TOGGLE_MODIFIER);
    Native::GetInstance()->RegisterHotKey(current_config->TOGGLE_KEY,
                                          current_config->TOGGLE_MODIFIER);

    for (auto i = 0; i < 4; i++) {
        current_config->RIGHT_STICK_KEYS[i] =
            glfwGetKeyScancode(current_config->RIGHT_STICK_KEYS[i]);
    }

    if (current_config->LEFT_MOUSE_KEY)
        current_config->LEFT_MOUSE_KEY = glfwGetKeyScancode(current_config->LEFT_MOUSE_KEY);
    if (current_config->RIGHT_MOUSE_KEY)
        current_config->RIGHT_MOUSE_KEY = glfwGetKeyScancode(current_config->RIGHT_MOUSE_KEY);
    if (current_config->MIDDLE_MOUSE_KEY)
        current_config->MIDDLE_MOUSE_KEY = glfwGetKeyScancode(current_config->MIDDLE_MOUSE_KEY);

    KeyboardManager::GetInstance()->SetPersistentMode(current_config->PERSISTANT_KEY_PRESS);
}

void Application::TogglePanning() {
    if (!panning_started_) {
        if (Config::Current()->AUTO_FOCUS_EMU_WINDOW) {
            Native::GetInstance()->SetFocusOnWindow(Config::Current()->TARGET_NAME);
        }

        const GLFWvidmode* videoMode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        screen_center_x_ = videoMode->width / 2, screen_center_y_ = videoMode->height / 2;

        Native::GetInstance()->SetMousePos(screen_center_x_, screen_center_y_);

        panning_started_ = true;
        glfwIconifyWindow(main_window_);
    }
    else {
        panning_started_ = false;
        controller_->ClearState();
        UpdateMouseVisibility(GetTotalRunningTime());
    }
}

void Application::DetectMouseMove() {
    static int last_cursor_x = 0, last_cursor_y = 0;

    int x = 0, y = 0;
    Native::GetInstance()->GetMousePos(&x, &y);

    if (x != last_cursor_x || y != last_cursor_y) {
        last_cursor_x = x;
        last_cursor_y = y;
        OnMouseMove(x, y);
    }

    if (Config::Current()->HIDE_MOUSE) {
        UpdateMouseVisibility();
    }
}

void Application::OnHotkey(HotkeyEvent& evt) {
    auto app = Application::GetInstance();
    DEBUG_OUT("hot_key: (%d, %d)\n", evt.key, evt.modifier);
#if _DEBUG
    if ((int)evt.key == glfwGetKeyScancode(GLFW_KEY_T) &&
        (int)evt.modifier == glfwGetKeyScancode(GLFW_KEY_LEFT_CONTROL)) {
        app->mouse_->TurnTest(app->main_view_->test_delay, app->main_view_->test_type);
    }
#endif
    if (evt.key == Config::Current()->TOGGLE_KEY &&
        evt.modifier == Config::Current()->TOGGLE_MODIFIER)
        app->TogglePanning();
}

void Application::OnMouseButton(MouseButtonEvent& evt) {
    static uint32_t last_key = 0;
    DEBUG_OUT("[%f] button: %d, pressed: %d\n", GetTotalRunningTime(), evt.key, evt.is_pressed);
    if (!Config::Current()->BIND_MOUSE_BUTTON) {
        return;
    }

    uint32_t key = 0;
    switch (evt.key) {
    case MOUSE_LBUTTON:
        key = Config::Current()->LEFT_MOUSE_KEY;
        break;
    case MOUSE_RBUTTON:
        key = Config::Current()->RIGHT_MOUSE_KEY;
        break;
    case MOUSE_MBUTTON:
        key = Config::Current()->MIDDLE_MOUSE_KEY;
        break;
    }
    if (key) {
        const std::string& target_window_name = Config::Current()->TARGET_NAME;
        auto app = Application::GetInstance();
        // fixes the left button press issue when trying to focus on another window
        // other than the target window. (Mostly on windows..)
        if (!Native::GetInstance()->IsMainWindowActive(target_window_name)) {
            if (last_key) {
                app->controller_->ClearState();
                auto last_window = Native::GetInstance()->GetFocusedWindow();
                Native::GetInstance()->SetFocusOnWindow(target_window_name);
                Native::GetInstance()->SendKeysUp((uint32_t*)&last_key, 1);
                Native::GetInstance()->SendKeysUp((uint32_t*)&key, 1);
                Native::GetInstance()->SetFocusOnWindow(last_window);
                last_key = 0;
            }
            if (evt.is_pressed) {
                return;
            }
        }
        app->controller_->SetButton(key, evt.is_pressed);
        last_key = key;
    }
}

void Application::OnMouseMove(int x, int y) {
    auto app = Application::GetInstance();
    if (app->panning_started_) {
        app->mouse_->MouseMoved(x, y, screen_center_x_, screen_center_y_);
        Native::GetInstance()->SetMousePos(screen_center_x_, screen_center_y_);
    }
    else {
        app->UpdateMouseVisibility(GetTotalRunningTime());
    }
}

void Application::UpdateMouseVisibility(double new_moved_time) {
    constexpr double default_mouse_hide_timeout = 2500;
    static double last_mouse_moved = GetTotalRunningTime();

    if (new_moved_time > 0.0) {
        last_mouse_moved = new_moved_time;
        Native::GetInstance()->CursorHide(false);
        return;
    }

    double current_time = GetTotalRunningTime();
    double time_passed = current_time - last_mouse_moved;
    if (time_passed >= default_mouse_hide_timeout) {
        Native::GetInstance()->CursorHide(
            Native::GetInstance()->IsMainWindowActive(Config::Current()->TARGET_NAME));
        last_mouse_moved = current_time;
    }
}

void Application::OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)action;
    if (action != GLFW_PRESS)
        return;
    GetInstance()->main_view_->OnKeyPress(key, scancode, mods);
}