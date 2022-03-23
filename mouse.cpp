#include "mouse.h"
#include "Config.h"
#include "native.h"
#include "Application.h"
#include "Utils.h"
#include <thread>
#include <cmath>

void Mouse::MouseMoved(double x, double y, double center_x, double center_y)
{
    auto mouse_change = vd2d{ x, y } - vd2d{ center_x, center_y };

    const auto move_distance = mouse_change.mag();
    mouse_panning_timeout_ = 0;

	if (move_distance == 0)
	{
		return;
	}

    /* found these values by testing with PLA only lmao somebody help :") */
    const auto update_time = Config::Current()->CAMERA_UPDATE_TIME;

    /*const auto last_move_distance = last_mouse_change.mag();
    auto angle = std::asin(std::abs(last_mouse_change.y) / last_move_distance) * 180 / 3.141593;

    auto time = angle * (angle_update_time / 90.0);*/

    const auto axis_change = mouse_change.norm() * update_time * 0.96;

    last_axis_change_ = (last_axis_change_ * 0.91) + (axis_change * 0.09);

    if (last_axis_change_.mag() < 1.0)
    {
        last_axis_change_ = axis_change / axis_change.mag();
    }

#if _DEBUG
    fprintf(stdout, "axis_change: %f, %f\n", axis_change.x, axis_change.y);
#endif
}

void Mouse::TurnTest(int delay, int test_type)
{
    switch (test_type)
    {
    case 0:
    {
        for (int j = 0; j < 4; j++)
        {
            Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[j], 1);
            std::this_thread::sleep_for(std::chrono::duration<double, std::ratio<1, 1000>>(delay));
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[j], 1);
        }
        break;
    }
    case 1:
    {
        Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[0], 1);
        std::this_thread::sleep_for(std::chrono::duration<double, std::ratio<1, 1000>>(delay));
        Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[0], 1);
        break;
    }
    case 2:
    {
        Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[2], 1);
        std::this_thread::sleep_for(std::chrono::duration<double, std::ratio<1, 1000>>(delay));
        Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[2], 1);
        break;
    }
    }
}

void Mouse::Update()
{
    const auto update_time = 10;
    const auto sensitivity = Config::Current()->SENSITIVITY * 0.022;
    
    for (int i = 0; i < 4; i++)
    {
        axis_change_timeouts_[i] *= sensitivity;
    }
    last_axis_change_ *= sensitivity;

    if (mouse_panning_timeout_++ > 20)
    {
        StopPanning();
    }

    SetCamera(sensitivity);

    std::this_thread::sleep_for(std::chrono::milliseconds(update_time));
}

void Mouse::StopPanning()
{
    last_axis_change_ = {};
    memset(axis_change_timeouts_, 0, sizeof(double) * 4);
    for (int i = 0; i < 4; i++)
    {
        if (release_timeouts_[i] > 0.0)
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[i], 1);
            release_timeouts_[i] = 0.0;
        }
    }
}

void Mouse::SetCamera(const double& sensitivity)
{
    const auto dir = last_axis_change_.norm();
    int direction_x = Utils::sign(last_axis_change_.x);
    int direction_y = Utils::sign(last_axis_change_.y);

    if (direction_x != 0.0 && (dir.x < -0.01 || dir.x > 0.5))
    {
        int current_axis_timeout = direction_x == 1;
        int opposite_axis_timeout = direction_x != 1;

        axis_change_timeouts_[opposite_axis_timeout] = 0.0;
        release_timeouts_[opposite_axis_timeout] = 0.1;

        axis_change_timeouts_[current_axis_timeout] += (std::abs(last_axis_change_.x) - axis_change_timeouts_[current_axis_timeout]);
    }

    if (direction_y != 0.0 && (dir.y < -0.01 || dir.y > 0.5))
    {
        int current_axis_timeout = 2 + (direction_y == 1);
        int opposite_axis_timeout = 2 + (direction_y != 1);

        axis_change_timeouts_[opposite_axis_timeout] = 0.0;
        release_timeouts_[opposite_axis_timeout] = 0.1;

        axis_change_timeouts_[current_axis_timeout] += (std::abs(last_axis_change_.y) - axis_change_timeouts_[current_axis_timeout]);
    }

    for (int i = 0; i < 4; i++)
    {
        if (axis_change_timeouts_[i] > sensitivity)
        {
            if (release_timeouts_[i] == 0.0)
            {
                Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[i], 1);
                release_timeouts_[i] = Application::GetTotalRunningTime() + axis_change_timeouts_[i];
            }
            else
            {
                release_timeouts_[i] += axis_change_timeouts_[i];
            }
        }
        else if (release_timeouts_[i] > 0.0 && release_timeouts_[i] < Application::GetTotalRunningTime())
        {
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[i], 1);
            release_timeouts_[i] = 0.0;
        }
    }
}
