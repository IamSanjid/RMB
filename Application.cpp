#include "Application.h"

#ifdef _MSC_VER
#include "win_native.h"
#else
#include "linux_native.h"
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

#include <GLFW/glfw3.h>

#if _WIN32 || _WIN64
#if _WIN64
#pragma comment(lib, "glfw/lib-vc2019-64/glfw3.lib")
#else
#error Only 64bit is supported.
#endif
#endif

#include "Config.h"
#include "views/MainView.h"
#include "mouse.h"
#include "controller.h"

#include <thread>
#include <stdio.h>

std::unique_ptr<Application> Application::Create()
{
    auto app = std::make_unique<Application>();
    if (app->Initialize(Config::Current()->NAME, Config::Current()->WIDTH, Config::Current()->HEIGHT))
        return std::move(app);
    return nullptr;
}

Application* Application::GetInstance()
{
    return instance_;
}

double Application::GetTotalRunningTime()
{
    typedef std::chrono::high_resolution_clock Time;
    typedef std::chrono::duration<double, std::milli> fmsec;

    static auto starting_time = Time::now().time_since_epoch();

    auto current_time = Time::now().time_since_epoch();

    return std::chrono::duration_cast<fmsec>(current_time - starting_time).count();
}

Application* Application::instance_ = nullptr;

Application::Application()
{
    instance_ = this;
    GetTotalRunningTime();
    EventBus::Instance().subscribe(this, &Application::OnHotkey);
}

Application::~Application()
{
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
    instance_ = nullptr;
    mouse_ = nullptr;
    main_view_ = nullptr;
}

#ifdef _DEBUG
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}
#endif

bool Application::Initialize(const char* name, uint32_t width, uint32_t height)
{
#ifdef _DEBUG
    glfwSetErrorCallback(glfw_error_callback);
#endif
    if (!glfwInit())
        return false;

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
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
    main_window_ = glfwCreateWindow(width, height, name, NULL, NULL);
    if (main_window_ == NULL)
        return false;
    
    controller_ = new Controller();
    mouse_ = new Mouse();
    
    glfwMakeContextCurrent(main_window_);
    glfwSwapInterval(1);
    glfwSetKeyCallback(main_window_, OnKeyCallback);

    Reconfig();
#if _DEBUG
    Native::GetInstance()->RegisterHotKey(glfwGetKeyScancode(GLFW_KEY_T), glfwGetKeyScancode(GLFW_KEY_LEFT_CONTROL));
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    main_view_ = new MainView();
    return ImGui_ImplGlfw_InitForOpenGL(main_window_, true) && ImGui_ImplOpenGL3_Init(glsl_version);
}

void Application::Run()
{
    if (is_running_)
        return;

    is_running_ = true;

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;
    ImGui::StyleColorsDark();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    std::thread([this]
    {
        while (is_running_)
        {
            controller_->Update();
            mouse_->Update(controller_);
        }
    }).detach();

    while (!glfwWindowShouldClose(main_window_))
    {
        Native::GetInstance()->Update();
        EventBus::Instance().update();

        glfwPollEvents();

        if (panning_started_)
        {
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
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(main_window_);
    }
}

void Application::Reconfig()
{
    /* getting scan codes for current platform */
    Native::GetInstance()->UnregisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
    Config::Current(Config::Default());

    Config::Current()->TOGGLE_KEY = glfwGetKeyScancode(Config::Current()->TOGGLE_KEY);
    Config::Current()->TOGGLE_MODIFIER = glfwGetKeyScancode(Config::Current()->TOGGLE_MODIFIER);
    Native::GetInstance()->RegisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);

    for (auto i = 0; i < 4; i++)
    {
        Config::Current()->RIGHT_STICK_KEYS[i] = glfwGetKeyScancode(Config::Current()->RIGHT_STICK_KEYS[i]);
    }
}

void Application::TogglePanning()
{
    if (!panning_started_)
    {
        if (Config::Current()->AUTO_FOCUS_RYU && !Native::GetInstance()->SetFocusOnProcess("Ryujinx"))
            return;
        panning_started_ = true;
        std::thread(&Application::StartPanning, this).detach();
#if !defined(_DEBUG)
        if (Config::Current()->HIDE_MOUSE)
            Native::GetInstance()->CursorHide(true);
#endif
        glfwIconifyWindow(main_window_);
    }
    else
    {
        panning_started_ = false;
        mouse_->StopPanning();
        Native::GetInstance()->CursorHide(false);
    }
}

void Application::StartPanning()
{
    int monitorX, monitorY;

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(monitor);

    glfwGetMonitorPos(monitor, &monitorX, &monitorY);

    int center_x = videoMode->width / 2, center_y = videoMode->height / 2;

    /* thread looks for mouse position change..  */
    int last_cursor_x = 0, last_cursor_y = 0;
    bool mouse_change_started = false;

    while (panning_started_)
    {
        int x = 0, y = 0;
        Native::GetInstance()->GetMousePos(&x, &y);

        if (x != last_cursor_x || y != last_cursor_y)
        {
            last_cursor_x = x; last_cursor_y = y;
            Native::GetInstance()->SetMousePos(center_x, center_y);
            if (mouse_change_started)
                mouse_->MouseMoved(x, y, center_x, center_y);
            mouse_change_started = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Application::OnHotkey(HotkeyEvent* evt)
{
#if _DEBUG
    fprintf(stdout, "hot_key: (%d, %d)\n", evt->key, evt->modifier);
    if ((int)evt->key == glfwGetKeyScancode(GLFW_KEY_T) && (int)evt->modifier == glfwGetKeyScancode(GLFW_KEY_LEFT_CONTROL))
    {    
        mouse_->TurnTest(main_view_->test_delay, main_view_->test_type);
    }
#endif
    if (evt->key == Config::Current()->TOGGLE_KEY && evt->modifier == Config::Current()->TOGGLE_MODIFIER)
        TogglePanning();
}

void Application::OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)window;
    (void)action;
    if (action != GLFW_PRESS)
        return;
    instance_->main_view_->OnKeyPress(key, scancode, mods);
}
