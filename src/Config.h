#pragma once
#include <cstdint>
#include <string>

class Config {
public:
    static Config* Current(Config* change = nullptr);
    static Config* LoadNew(const std::string& file);

    Config();

    void Save(const std::string& file);

    const char* NAME = "RMB";
    const uint32_t WIDTH = 420;
    const uint32_t HEIGHT = 520;

    std::string TARGET_NAME = "Ryujinx";

    int TOGGLE_MODIFIER;
    int TOGGLE_KEY;

    int RIGHT_STICK_KEYS[4]{-1,-1,-1,-1};
    int LEFT_MOUSE_KEY;
    int RIGHT_MOUSE_KEY;
    int MIDDLE_MOUSE_KEY;

    float SENSITIVITY;

    bool HIDE_MOUSE;
    bool AUTO_FOCUS_EMU_WINDOW;
    bool BIND_MOUSE_BUTTON;
    bool PERSISTANT_KEY_PRESS;

    float DEADZONE = 0.15f;
    float RANGE = 0.95f;
    float THRESHOLD = 0.5f;
    float X_OFFSET = 0.0f;
    float Y_OFFSET = 0.0f;
};