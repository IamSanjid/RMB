#pragma once
#include <mutex>
#include <thread>
#include <vector>

struct Axes {
    float x;
    float y;

    bool left;
    bool right;
    bool up;
    bool down;
};

class InputHandler;
class ButtonInputHandler;
class StickInputHandler;

class NpadController {
public:
    NpadController();

    void SetStick(float raw_x, float raw_y);
    void SetButton(uint32_t button, int value);
    void ClearState();

private:
    void UpdateThread(std::stop_token stop_token);
    void SanatizeAxes(float raw_x, float raw_y, bool clamp_value);

    StickInputHandler* stick_handler_;
    ButtonInputHandler* button_input_handler_;

    float last_raw_x_;
    float last_raw_y_;

    float last_x_;
    float last_y_;

    Axes axes_{};

    std::jthread update_thread;
    mutable std::mutex mutex;
};