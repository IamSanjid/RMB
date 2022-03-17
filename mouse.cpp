#include "mouse.h"
#include "Config.h"
#include "native.h"
#include "Application.h"
#include <thread>
#include <cmath>

void Mouse::MouseMoved(double x, double y, double center_x, double center_y)
{
    auto mouse_change = vd2d{ x, y } - vd2d{ center_x, center_y };

    const auto move_distance = mouse_change.mag();

	if (move_distance == 0 || key_up_time_ > 0.0)
	{
		return;
	}

    /* found these values by testing with PLA only lmao somebody help :") */
    const auto update_time = Config::Current()->CAMERA_UPDATE_TIME;
    const auto sensitivity = Config::Current()->SENSITIVITY * 0.022;

    /*const auto last_move_distance = last_mouse_change.mag();
    auto angle = std::asin(std::abs(last_mouse_change.y) / last_move_distance) * 180 / 3.141593;

    auto time = angle * (angle_update_time / 90.0);*/

    const auto dir = mouse_change.norm();
    const auto x_percentage = (std::abs(mouse_change.x) / move_distance) * 0.96;
    const auto y_percentage = (std::abs(mouse_change.y) / move_distance) * 0.96;

    double time = 0.0;

    if (dir.x < -0.01)
    {
        time += update_time * x_percentage * sensitivity;
        keys_.push_back(Config::Current()->RIGHT_STICK_KEYS[0]);
    }
    else if (dir.x > 0.5)
    {
        time += update_time * x_percentage * sensitivity;
        keys_.push_back(Config::Current()->RIGHT_STICK_KEYS[1]);
    }

    if (dir.y < -0.01)
    {
        time += update_time * y_percentage * sensitivity;
        keys_.push_back(Config::Current()->RIGHT_STICK_KEYS[2]);
    }
    else if (dir.y > 0.5)
    {
        time += update_time * y_percentage * sensitivity;
        keys_.push_back(Config::Current()->RIGHT_STICK_KEYS[3]);
    }
    
    if (time > 0.0)
    {
        key_up_time_ = Application::GetTotalRunningTime() + time;
        Native::GetInstance()->SendKeysDown(&keys_[0], keys_.size());
    }

#if _DEBUG
    fprintf(stdout, "mouse changed x: %f , y: %f - distance: %f, time: %f, keys: %d\n", mouse_change.x, mouse_change.y, mouse_change.mag(), time, (int)keys_.size());
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
    const auto update_time = 1.0;
    
    if (keys_.size() > 0 && key_up_time_ < Application::GetTotalRunningTime())
    {
        Native::GetInstance()->SendKeysUp(&keys_[0], keys_.size());
        key_up_time_ = 0.0;
        keys_.clear();
    }

    std::this_thread::sleep_for(std::chrono::duration<double, std::ratio<1, 1000>>(update_time));
}
