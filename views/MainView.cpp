#include "MainView.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "../Application.h"
#include "../Config.h"
#include "../native.h"

MainView::MainView()
{
    IMGUI_CHECKVERSION();
    name_ = "Main";

    glfw_modifiers = new uint32_t[]
    {
        (uint32_t)glfwGetKeyScancode(GLFW_KEY_LEFT_CONTROL),
        (uint32_t)glfwGetKeyScancode(GLFW_KEY_LEFT_SHIFT),
        (uint32_t)glfwGetKeyScancode(GLFW_KEY_LEFT_ALT)
    };

    for (int f_key = GLFW_KEY_F1; f_key <= GLFW_KEY_F25; f_key++)
    {
        int scan_code = glfwGetKeyScancode(f_key);
        if (scan_code)
        {
            std::string str = ("F" + std::to_string(f_key + 1 - GLFW_KEY_F1));
            char* new_str = new char[str.length() + 1]{ 0 };
#ifdef _MSC_VER
            strcpy_s(new_str, str.length() + 1, str.c_str());
#else
            strcpy(new_str, str.c_str());
#endif
            glfw_str_keys.push_back(new_str);
            glfw_keys.push_back(scan_code);
        }
    }

    glfw_str_keys.push_back("SPACE");
    glfw_keys.push_back(glfwGetKeyScancode(GLFW_KEY_SPACE));

    for (auto key = GLFW_KEY_COMMA; key <= GLFW_KEY_GRAVE_ACCENT; key++)
    {
        int scanCode = glfwGetKeyScancode(key);
        if (scanCode > 0)
        {
            auto str = glfwGetKeyName(key, scanCode);
            if (str)
            {
                glfw_str_keys.push_back(str);
                glfw_keys.push_back(scanCode);
            }
        }
    }

    GetRightStickButtons();
}

MainView::~MainView()
{
    delete[] glfw_modifiers;
}

void MainView::Show()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = NULL;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);
    
    ImGui::Begin(name_, 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
    
    if (ImGui::Button(Application::GetInstance()->IsPanning() ? "Stop" : "Start"))
    {
        Application::GetInstance()->TogglePanning();
    }

    /* panning toggle hotkey changer.. */
    ImGui::TextDisabled("Panning Toggle Hotkeys");
    ImGui::Text("Modifier: ");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##modifier_combo", modifiers[modifier_selected]))
    {
        for (int n = 0; n < IM_ARRAYSIZE(modifiers); n++)
        {
            bool is_selected = modifier_selected == n;
            if (ImGui::Selectable(modifiers[n], is_selected))
                modifier_selected = n;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (Config::Current()->TOGGLE_MODIFIER != glfw_modifiers[modifier_selected])
    {
        Native::GetInstance()->UnregisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
        Config::Current()->TOGGLE_MODIFIER = glfw_modifiers[modifier_selected];
        Native::GetInstance()->RegisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
    }

    ImGui::Text("Key: ");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##key_combo", glfw_str_keys[key_selected]))
    {
        for (int n = 0; n < glfw_str_keys.size(); n++)
        {
            bool is_selected = key_selected == n;
            if (ImGui::Selectable(glfw_str_keys[n], is_selected))
                key_selected = n;
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (Config::Current()->TOGGLE_KEY != glfw_keys[key_selected])
    {
        Native::GetInstance()->UnregisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
        Config::Current()->TOGGLE_KEY = glfw_keys[key_selected];
        Native::GetInstance()->RegisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
    }

    ImGui::NewLine();

    if (ImGui::InputDouble("Sensitivity", &Config::Current()->SENSITIVITY, 0.5, 0.0, "%0.3f"))
    {
        if (Config::Current()->SENSITIVITY < 0.50)
            Config::Current()->SENSITIVITY = 0.50;
    }

    ImGui::Text("Camera Update Time(ms):");
    if (ImGui::InputDouble(" ", &Config::Current()->CAMERA_UPDATE_TIME, 10.0, 0.0, "%0.3f"))
    {
        if (Config::Current()->CAMERA_UPDATE_TIME < 100.0)
            Config::Current()->CAMERA_UPDATE_TIME = 100.0;
    }

    ImGui::NewLine();

    if (ImGui::Button("Configure Right Stick"))
    {
        show_change_r_stick = !show_change_r_stick;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Make sure you select the correct right stick up/down/left/right keys which are\nbinded with your keyboard.");

    ImGui::SameLine();

    if (ImGui::Button("Default"))
    {
        modifier_selected = 0; key_selected = 8;
        Application::GetInstance()->Reconfig();
        GetRightStickButtons();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Resets to default configuration.");

    /* configure right stick buttons */
    if (show_change_r_stick)
    {
        ImGui::BeginChild("conf_right_stick", ImVec2(ImGui::GetWindowSize().y, 85), true);
        auto windowWidth = ImGui::GetWindowSize().x;
        bool bounded = false;
        for (int i = 2; i < 4; i++)
        {
            if (i == 2 && bounded)
                continue;

            auto textWidth = ImGui::CalcTextSize(r_btn_text_[i].c_str()).x;
            if (i == 2 || i == 3)
                ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
            else if (i == 1)
                ImGui::SameLine((windowWidth - textWidth) * 0.95f);

            bool others_changing = false;
            for (int j = 0; j < 4; j++)
            {
                if (j != i && r_btn_changing_[j])
                {
                    others_changing = true;
                    break;
                }
            }

            if (ImGui::Button(r_btn_text_[i].c_str()) && !others_changing)
            {
                r_btn_changing_[i] = !r_btn_changing_[i];
                if (r_btn_changing_[i])
                    r_btn_text_[i] = "waiting..";
                else
                {
                    auto str = glfwGetKeyName(r_btn_key_codes_[i], Config::Current()->RIGHT_STICK_KEYS[i]);
                    if (str)
                        r_btn_text_[i] = std::string(str);
                    else
                        r_btn_text_[i] = std::to_string(Config::Current()->RIGHT_STICK_KEYS[i]);
                }
            }

            if (ImGui::IsItemHovered())
            {
                if (i == 0)
                    ImGui::SetTooltip("Left button");
                else if (i == 1)
                    ImGui::SetTooltip("Right button");
                else if (i == 2)
                    ImGui::SetTooltip("Up button");
                else if (i == 3)
                    ImGui::SetTooltip("Down button");
            }

            if (i == 2)
            {
                i = -1;
                bounded = true;
            }
        }

        ImGui::EndChild();
    }

#if _DEBUG
    ImGui::InputInt("Test Delay", &test_delay, 10);
    if (test_delay < 10)
        test_delay = 10;
    ImGui::InputInt("Test Type", &test_type, 1);
    if (test_type < 0 || test_type > 2)
        test_type = 0;
#endif
    ImGui::End();
}

void MainView::OnKeyPress(int key, int scancode, int mods)
{
    (void)mods;
    for (int i = 0; i < 4; i++)
    {
        if (r_btn_changing_[i])
        {
            r_btn_changing_[i] = false;
            auto str = glfwGetKeyName(key, scancode);
            if (str)
                r_btn_text_[i] = std::string(str);
            else
                r_btn_text_[i] = std::to_string(key);
            r_btn_key_codes_[i] = key;
            Config::Current()->RIGHT_STICK_KEYS[i] = scancode;
            break;
        }
    }
}

void MainView::GetRightStickButtons()
{
    for (auto key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++)
    {
        int scanCode = glfwGetKeyScancode(key);
        auto str = glfwGetKeyName(key, scanCode);
        for (auto i = 0; i < 4; i++)
        {
            if ((int)Config::Current()->RIGHT_STICK_KEYS[i] == scanCode)
            {
                r_btn_key_codes_[i] = key;
                if (str)
                    r_btn_text_[i] = std::string(str);
                else
                    r_btn_text_[i] = std::to_string(key);
            }
        }
    }
}
