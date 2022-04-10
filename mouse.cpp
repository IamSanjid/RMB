#include "mouse.h"
#include <thread>
#include <cmath>

#include "Config.h"
#include "controller.h"
#include "Utils.h"

#if _DEBUG
#include "native.h"
#endif

void Mouse::MouseMoved(int x, int y, int center_x, int center_y)
{
	auto mouse_change = vf2d{ static_cast<float>(x), static_cast<float>(y) } - vf2d{ static_cast<float>(center_x), static_cast<float>(center_y) };

	const auto move_distance = mouse_change.mag();
	mouse_panning_timeout_ = 0;

	if (move_distance == 0)
	{
		return;
	}

	/*const auto last_move_distance = last_mouse_change.mag();
	auto angle = std::asin(std::abs(last_mouse_change.y) / last_move_distance) * 180 / 3.141593;

	auto time = angle * (angle_update_time / 90.0);*/
	/* copied from yuzu: https://github.com/yuzu-emu/yuzu/blob/bf3c6f88126d0167329c4a18759cdabc7584f8b3/src/input_common/drivers/mouse.cpp#L74 */

	if (move_distance < 3.0f)
	{
		mouse_change /= move_distance;
		mouse_change *= 3.0f;
	}

	int x_dir = Utils::sign(mouse_change.x);
	int y_dir = Utils::sign(mouse_change.y);

	bool x_dir_changed = x_dir != 0 && Utils::sign(last_mouse_change_.x) != x_dir;
	bool y_dir_changed = y_dir != 0 && Utils::sign(last_mouse_change_.y) != y_dir;

	last_mouse_change_ = (last_mouse_change_ * 0.91f) + (mouse_change * 0.09f);

	if (x_dir_changed)
	{
		last_mouse_change_.x = last_mouse_change_.x / last_mouse_change_.mag();
	}

	if (y_dir_changed)
	{
		last_mouse_change_.y = last_mouse_change_.y / last_mouse_change_.mag();
	}

	const auto last_move_distance = last_mouse_change_.mag();

	if (last_move_distance > 8.0f)
	{
		last_mouse_change_ /= last_move_distance;
		last_mouse_change_ *= 8.0f;
	}

	if (last_move_distance < 1.0f)
	{
		last_mouse_change_ = mouse_change / mouse_change.mag();
	}

#if _DEBUG
	fprintf(stdout, "current change: %f, %f - avg change: %f, %f\n", mouse_change.x, mouse_change.y, last_mouse_change_.x, last_mouse_change_.y);
#endif

}

#if _DEBUG
void Mouse::TurnTest(int delay, int test_type)
{
	switch (test_type)
	{
	case 0:
	{
		MouseMoved(10, 11, 11, 11);
		break;
	}
	case 1:
	{
		Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[0], 1);
		std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(static_cast<double>(delay)));
		Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[0], 1);

		break;
	}
	case 2:
	{
		Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[2], 1);
		std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(static_cast<double>(delay)));
		Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[2], 1);
		break;
	}
	}
}
#endif

void Mouse::Update(Controller* controller)
{
	constexpr auto update_time = 10;
	const float sensitivity = Config::Current()->SENSITIVITY * 0.025f;

	last_mouse_change_ *= 0.8f;

	controller->SetAxis(last_mouse_change_.x * sensitivity, last_mouse_change_.y * sensitivity);

	if (mouse_panning_timeout_++ > 20)
	{
		StopPanning();
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(update_time));
}

void Mouse::StopPanning()
{
	last_mouse_change_ = {};
}
