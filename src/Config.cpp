#include <GLFW/glfw3.h>
#include <iniparser.hpp>
#include "Config.h"

Config::Config() {
    constexpr uint32_t NONE = 0x0;

    TOGGLE_MODIFIER = GLFW_KEY_LEFT_CONTROL;
    TOGGLE_KEY = GLFW_KEY_F9;

    /* default ryujinx right stick keys j = left, l = right, i = up, k = down */
    RIGHT_STICK_KEYS[0] = GLFW_KEY_J;
    RIGHT_STICK_KEYS[1] = GLFW_KEY_L;
    RIGHT_STICK_KEYS[2] = GLFW_KEY_I;
    RIGHT_STICK_KEYS[3] = GLFW_KEY_K;

    LEFT_MOUSE_KEY = NONE;
    RIGHT_MOUSE_KEY = NONE;
    MIDDLE_MOUSE_KEY = NONE;

    SENSITIVITY = 10.0f;

    HIDE_MOUSE = true;
    AUTO_FOCUS_EMU_WINDOW = true;
    BIND_MOUSE_BUTTON = true;
    PERSISTANT_KEY_PRESS = false;
}

Config* Config::Current(Config* change) {
    static Config* current_instance = new Config();
    if (change) {
        if (current_instance) {
            delete current_instance;
        }
        current_instance = change;
    }
    return current_instance;
}

Config* Config::LoadNew(const std::string& file) {
    INI::File ft;
    if (!ft.Load(file))
        return nullptr;

    auto new_conf = new Config();

    new_conf->TARGET_NAME =
        ft.GetValue("TargetEmulatorWindow", Config::Current()->TARGET_NAME).AsString();

    new_conf->SENSITIVITY = ft.GetValue("Sensitivity", Config::Current()->SENSITIVITY).AsT<float>();

    new_conf->HIDE_MOUSE = ft.GetValue("HideMouse", Config::Current()->HIDE_MOUSE).AsBool();
    new_conf->AUTO_FOCUS_EMU_WINDOW =
        ft.GetValue("AutoFocusEmuWindow", Config::Current()->AUTO_FOCUS_EMU_WINDOW).AsBool();
    new_conf->BIND_MOUSE_BUTTON =
        ft.GetValue("BindMouseButton", Config::Current()->BIND_MOUSE_BUTTON).AsBool();
    new_conf->PERSISTANT_KEY_PRESS =
        ft.GetValue("PersistantKeyPress", Config::Current()->PERSISTANT_KEY_PRESS).AsBool();

    new_conf->DEADZONE =
        ft.GetValue("AnalogProperties:DeadZone", Config::Current()->DEADZONE).AsT<float>();
    new_conf->RANGE = ft.GetValue("AnalogProperties:Range", Config::Current()->RANGE).AsT<float>();
    new_conf->THRESHOLD =
        ft.GetValue("AnalogProperties:Threshold", Config::Current()->THRESHOLD).AsT<float>();
    new_conf->X_OFFSET =
        ft.GetValue("AnalogProperties:XOffset", Config::Current()->X_OFFSET).AsT<float>();
    new_conf->Y_OFFSET =
        ft.GetValue("AnalogProperties:YOffset", Config::Current()->Y_OFFSET).AsT<float>();

    for (int i = 0; i < 4; i++) {
        std::string key = "RightStick:" + std::to_string(i);
        new_conf->RIGHT_STICK_KEYS[i] = ft.GetValue(key, Config::Current()->RIGHT_STICK_KEYS[i]).AsInt();
    }

    new_conf->LEFT_MOUSE_KEY =
        ft.GetValue("Mouse:LeftButton", Config::Current()->LEFT_MOUSE_KEY).AsInt();
    new_conf->RIGHT_MOUSE_KEY =
        ft.GetValue("Mouse:RightButton", Config::Current()->RIGHT_MOUSE_KEY).AsInt();
    new_conf->MIDDLE_MOUSE_KEY =
        ft.GetValue("Mouse:MiddleButton", Config::Current()->MIDDLE_MOUSE_KEY).AsInt();

    new_conf->TOGGLE_MODIFIER =
        ft.GetValue("PanningToggle:Modifier", Config::Current()->TOGGLE_MODIFIER).AsInt();
    new_conf->TOGGLE_KEY = ft.GetValue("PanningToggle:Key", Config::Current()->TOGGLE_KEY).AsInt();

    return new_conf;
}

void Config::Save(const std::string& file) {
    INI::File ft;
    ft.Load(file);

    ft.SetValue("TargetEmulatorWindow", this->TARGET_NAME);

    ft.SetValue("Sensitivity", this->SENSITIVITY);

    ft.SetValue("HideMouse", this->HIDE_MOUSE);
    ft.SetValue("AutoFocusEmuWindow", this->AUTO_FOCUS_EMU_WINDOW);
    ft.SetValue("BindMouseButton", this->BIND_MOUSE_BUTTON);
    ft.SetValue("PersistantKeyPress", this->PERSISTANT_KEY_PRESS);

    ft.SetValue("AnalogProperties:DeadZone", this->DEADZONE);
    ft.SetValue("AnalogProperties:Range", this->RANGE);
    ft.SetValue("AnalogProperties:Threshold", this->THRESHOLD);
    ft.SetValue("AnalogProperties:XOffset", this->X_OFFSET);
    ft.SetValue("AnalogProperties:YOffset", this->Y_OFFSET);

    for (int i = 0; i < 4; i++) {
        std::string key = "RightStick:" + std::to_string(i);
        ft.SetValue(key, this->RIGHT_STICK_KEYS[i]);
    }

    ft.SetValue("Mouse:LeftButton", this->LEFT_MOUSE_KEY);
    ft.SetValue("Mouse:RightButton", this->RIGHT_MOUSE_KEY);
    ft.SetValue("Mouse:MiddleButton", this->MIDDLE_MOUSE_KEY);

    ft.SetValue("PanningToggle:Modifier", this->TOGGLE_MODIFIER);
    ft.SetValue("PanningToggle:Key", this->TOGGLE_KEY);

    ft.Save(file);
}
