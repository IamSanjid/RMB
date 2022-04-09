#include "controller.h"
#include <stdio.h>
#include <stdint.h>
#include <cmath>
#include "native.h"
#include "Config.h"
#include "Utils.h"

constexpr int32_t HID_JOYSTICK_MAX = 0x7fff;

/* TODO: make these configurable */
const float deadzone = 0.15f;
const float range = 1.0f;
const float threshold = 0.5f;
const float offset = 0.0f;

constexpr int x_axis = 0;
constexpr int y_axis = 1;

class Device 
{
public:
    virtual void OnChange(int index, const State& state) = 0;
};

class AnalogDevice : public Device
{
public:
    void OnChange(int index, const State& state) override
    {
        int button = (index * 2) + (Utils::sign(state.value) == 1);
        if (!state.reset && !sent_buttons_[button])
        {
            Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[button], 1);
        }
        else if (state.reset && sent_buttons_[button])
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[button], 1);
        }
    }

private:
    bool sent_buttons_[BUTTONS]{};
};

Controller::Controller()
    : device(new AnalogDevice())
{
}

void Controller::Update()
{
    UpdateAxisState(x_axis, axes_.x);
    UpdateAxisState(y_axis, axes_.y);

    if (axes_.x != 0)
    {
        int last_x_sign = Utils::sign(axes_.x);
        axes_.x = last_x_sign * (std::abs(axes_.x) - 10);

        if (Utils::sign(axes_.x) != last_x_sign)
        {
            axes_.x = 0;
        }
    }

    if (axes_.y != 0)
    {
        int last_y_sign = Utils::sign(axes_.y);
        axes_.y = last_y_sign * (std::abs(axes_.y) - 10);

        if (Utils::sign(axes_.y) != last_y_sign)
        {
            axes_.y = 0;
        }
    }
}

void Controller::SetAxis(float raw_x, float raw_y)
{
    std::lock_guard<std::mutex> lock{ mutex };
	if (last_raw_x_ != raw_x || last_raw_y_ != raw_y)
	{
        //printf("new change: %f, %f\n", raw_x, raw_y);
		last_raw_x_ = raw_x;
		last_raw_y_ = raw_y;
		OnChange();
	}
}

void Controller::UpdateAxisState(int axis_index, int axis)
{
    const int limit = threshold * HID_JOYSTICK_MAX;
    const int update_time = static_cast<int>(Config::Current()->CAMERA_UPDATE_TIME);

    State& state = axis_index == x_axis ? x_state : y_state;

    if (axis == 0)
    {
        if (state.value != 0)
        {
            state.reset = true;
            device->OnChange(0, state);

            state.value = 0;
        }
        return;
    }

    int dir = Utils::sign(axis);
    int opposite_dir = -1 * Utils::sign(axis);

    if (Utils::sign(state.value) == opposite_dir)
    {
        state.reset = true;
        device->OnChange(axis_index, state);
    }

    if (std::abs(axis) > limit)
    {
        state.value = dir * (std::abs(state.value) + 1);
        state.reset = false;
        if (std::abs(state.value) > update_time)
        {
            device->OnChange(axis_index, state);
        }
    }
    else if (std::abs(state.value) > 0)
    {
        int last_state_sign = Utils::sign(state.value);
        state.value = last_state_sign * (std::abs(state.value) - 1);
        if (Utils::sign(state.value) != last_state_sign)
        {
            state.reset = true;
            state.value = last_state_sign;
            device->OnChange(0, state);

            state.value = 0;
        }
    }
}

void Controller::OnChange()
{
    SanatizeAxes(last_raw_x_, last_raw_y_, true);

    axes_.x = static_cast<int32_t>(last_x_ * HID_JOYSTICK_MAX);
    axes_.y = static_cast<int32_t>(last_y_ * HID_JOYSTICK_MAX);

    int x_dir = Utils::sign(axes_.x);
    int y_dir = Utils::sign(axes_.x);

    axes_.right = last_x_ > threshold;
    axes_.left = last_x_ < -threshold;
    axes_.up = last_y_ > threshold;
    axes_.down = last_y_ < -threshold;

#if _DEBUG
    /*if (axes_.x != 0 || axes_.y != 0)
    {
        fprintf(stdout, "axes_change: %d, %d - actual_value: %f, %f\n", axes_.x, axes_.y, last_x_, last_y_);
        fprintf(stdout, "right: %d left: %d up: %d down: %d\n", axes_.right, axes_.left, axes_.up, axes_.down);
    }*/
#endif
}

void Controller::SanatizeAxes(float raw_x, float raw_y, bool clamp_value)
{
    float& x = last_x_;
    float& y = last_y_;

    if (!std::isnormal(raw_x))
        raw_x = 0;
    if (!std::isnormal(raw_y))
        raw_y = 0;

    raw_x += offset;
    raw_y += offset;

    if (std::abs(offset) < 0.5f)
    {
        if (raw_x > 0) 
        {
            raw_x /= 1 + offset;
        }
        else 
        {
            raw_x /= 1 - offset;
        }
    }

    if (std::abs(offset) < 0.5f)
    {
        if (raw_y > 0) 
        {
            raw_y /= 1 + offset;
        }
        else 
        {
            raw_y /= 1 - offset;
        }
    }

    x = raw_x;
    y = raw_y;

    float r = x * x + y * y;
    r = std::sqrt(r);

    if (r <= deadzone || deadzone >= 1.0f)
    {
        x = 0;
        y = 0;
        return;
    }

    const float deadzone_factor =
        1.0f / r * (r - deadzone) / (1.0f - deadzone);
    x = x * deadzone_factor / range;
    y = y * deadzone_factor / range;
    r = r * deadzone_factor / range;

    if (clamp_value && r > 1.0f)
    {
        x /= r;
        y /= r;
    }
}
