#include "mouse.h"
#include "Config.h"
#include "native.h"
#include <thread>
#include <cmath>

void Mouse::MouseMoved(double x, double y, double center_x, double center_y)
{
    auto mouse_change = vd2d{ x, y } - vd2d{ center_x, center_y };

    const auto move_distance = mouse_change.mag();

	if (move_distance == 0)
	{
		return;
	}

    if (move_distance < 3.0f) 
    {
        mouse_change /= move_distance;
        mouse_change *= 3.0f;
    }

    if (move_distance > 8.0f) 
    {
        mouse_change /= move_distance;
        mouse_change *= 8.0f;
    }

    /* found these values by testing with PLA only lmao somebody help :") */
    const auto update_time = Config::Current()->CAMERA_UPDATE_TIME;
    const auto sensitivity = Config::Current()->SENSITIVITY * 0.022;

    /*const auto last_move_distance = last_mouse_change.mag();
    auto angle = std::asin(std::abs(last_mouse_change.y) / last_move_distance) * 180 / 3.141593;

    auto time = angle * (angle_update_time / 90.0);*/

    const auto dir = mouse_change.norm();
    const auto dir_diff_x = std::abs((dir.x * 0.96) - (dir.y * 0.96));
    const auto dir_diff_y = std::abs((dir.y * 0.96) - (dir.x * 0.96));

    auto press_key = [](double time, uint32_t key)
    {
        Native::GetInstance()->SendKeysDown(&key, 1);
        std::this_thread::sleep_for(std::chrono::duration<double, std::ratio<1, 1000>>(time));
        Native::GetInstance()->SendKeysUp(&key, 1);
    };

    if (dir.x < -0.01)
    {
        press_key(update_time * dir_diff_x * sensitivity, Config::Current()->RIGHT_STICK_KEYS[0]);
    }
    else if (dir.x > 0.5)
    {
        press_key(update_time * dir_diff_x * sensitivity, Config::Current()->RIGHT_STICK_KEYS[1]);
    }

    if (dir.y < -0.01)
    {
        press_key(update_time * dir_diff_y * sensitivity, Config::Current()->RIGHT_STICK_KEYS[2]);
    }
    else if (dir.y > 0.5)
    {
        press_key(update_time * dir_diff_y * sensitivity, Config::Current()->RIGHT_STICK_KEYS[3]);
    }


#if _DEBUG
    fprintf(stdout, "mouse changed x: %f , y: %f - distance: %f\n", std::abs(dir.x), std::abs(dir.y), mouse_change.mag());
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
