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

constexpr int left = 0;
constexpr int right = 1;
constexpr int up = 2;
constexpr int down = 3;

void Controller::Update()
{
    const auto limit = 1020;
    if (axes_.x == 0)
    {
        if (sent_axis_buttons_[right])
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[right], 1);
            sent_axis_buttons_[right] = false;
        }

        if (sent_axis_buttons_[left])
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[left], 1);
            sent_axis_buttons_[left] = false;
        }
    }
    else if (std::abs(axes_.x) >= limit)
    {
        fprintf(stdout, "x: %d\n", std::abs(axes_.x));
        int sign = Utils::sign(axes_.x);
        int current_button = sign == 1;
        int opposite_button = sign != 1;

        if (sent_axis_buttons_[opposite_button])
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[opposite_button], 1);
            sent_axis_buttons_[opposite_button] = false;
        }

        if (!sent_axis_buttons_[current_button])
        {
            Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[current_button], 1);
            sent_axis_buttons_[current_button] = true;
        }
    }

    if (axes_.y == 0)
    {
        if (sent_axis_buttons_[up])
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[up], 1);
            sent_axis_buttons_[up] = false;
        }

        if (sent_axis_buttons_[down])
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[down], 1);
            sent_axis_buttons_[down] = false;
        }
    }
    else if (std::abs(axes_.y) >= limit)
    {
        fprintf(stdout, "y: %d\n", std::abs(axes_.y));
        int sign = Utils::sign(axes_.y);
        int current_button = 2 + (sign == 1);
        int opposite_button = 2 + (sign != 1);

        if (sent_axis_buttons_[opposite_button])
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[opposite_button], 1);
            sent_axis_buttons_[opposite_button] = false;
        }

        if (!sent_axis_buttons_[current_button])
        {
            Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[current_button], 1);
            sent_axis_buttons_[current_button] = true;
        }
    }
    axes_.x = 0;
    axes_.y = 0;
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
