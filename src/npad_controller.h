#pragma once
#include <mutex>

struct Axes {
    float x;
    float y;

    bool left;
    bool right;
    bool up;
    bool down;
};

class StickInputHandler;

class NpadController {
public:
    NpadController();
    ~NpadController();

    void SetStick(float raw_x, float raw_y);
    void SetButton(uint32_t button, int value);
    void SetPersistentMode(bool value);
    void ClearState();

private:
    void SanatizeAxes(float raw_x, float raw_y, bool clamp_value);

    StickInputHandler* stick_handler_;

    float last_raw_x_{};
    float last_raw_y_{};

    float last_x_{};
    float last_y_{};

    Axes axes_{};

    mutable std::mutex mutex;
};