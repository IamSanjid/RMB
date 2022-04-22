#include "npad_controller.h"
#include <stdint.h>
#include <cmath>

#include "native.h"
#include "Config.h"
#include "Utils.h"
#include "Application.h"
#include "linked_queue.h"

constexpr int BUTTONS = 4;

/* TODO: make these configurable */
const float deadzone = 0.09f;
const float range = 1.0f;
const float threshold = 0.5f;
const float offset = 0.0f;

constexpr int x_axis = 0;
constexpr int y_axis = 1;

/* TODO: instead of hardcoding try to retrieve the index dynamically using Enum and indentifier/hashing */
constexpr int StickInputHandlerIndex = 0;
constexpr int ButtonInputHandlerIndex = 1;

class InputHandler
{
public:
	virtual void OnUpdate() = 0;
	virtual void OnChange(int index, const InputStatus& status) = 0;
	virtual void OnStop() = 0;
};

class StickInputHandler final : public InputHandler
{
public:
	void OnUpdate() override
	{
		float current_time = static_cast<float>(Application::GetTotalRunningTime());
		float time_passed = current_time - last_update_;

		last_update_ = current_time;

		float start_wasted_time = static_cast<float>(Application::GetTotalRunningTime());
		if (stopped_)
		{
			for (int i = 0; i < BUTTONS; i++)
			{
				if (timeouts_[i] > 0.0f)
				{
					Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[i], 1);
				}
				timeouts_[i] = 0.0f;
			}
			timeout_queue_.Clear();
			stopped_ = false;

			float end_wasted_time = static_cast<float>(Application::GetTotalRunningTime());
			last_update_ += end_wasted_time - start_wasted_time;
			return;
		}

		if (time_passed != last_update_)
		{
			for (int i = 0; i < BUTTONS; i++)
			{
				if (timeouts_[i] > 0.0f)
				{
					timeouts_[i] -= time_passed;
					if (timeouts_[i] <= 0.001f)
					{
						Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[i], 1);
						timeouts_[i] = 0.0f;
						continue;
					}
				}
			}
		}
		
		std::vector<ButtonTimeout> current_timeouts;
		if (timeout_queue_.Pop(current_timeouts))
		{
			for (auto& timeout : current_timeouts)
			{
				if (timeouts_[timeout.button] != timeout.time)
				{
					if (timeout.time <= 0.0f)
					{
						Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[timeout.button], 1);
					}
					else if (timeouts_[timeout.button] <= 0.0f)
					{
						Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[timeout.button], 1);
					}
					timeouts_[timeout.button] = timeout.time;
				}
			}
		}

		float end_wasted_time = static_cast<float>(Application::GetTotalRunningTime());
		last_update_ += end_wasted_time - start_wasted_time;
	}

	void OnChange(int index, const InputStatus& status) override
	{
		if (stopped_)
			return;

		int button = (index * 2) + (Utils::sign(status.value) == 1);
		int opposite_button = (index * 2) + (Utils::sign(status.value) != 1);
		float time = status.reset ? 0.0f : std::abs(status.value);

		timeout_queue_.Push({ button, time });
		timeout_queue_.Push({ opposite_button, 0.f });

	}

	void OnStop() override
	{
		stopped_ = true;
	}

private:
	struct ButtonTimeout
	{
		int button;
		float time;
	};

	float timeouts_[BUTTONS]{};
	float last_update_{};
	bool stopped_{};
	LinkedQueue<ButtonTimeout> timeout_queue_{};
};

class ButtonInputHandler final : public InputHandler
{
public:
	void OnUpdate() override
	{
		if (stopped_)
		{
			std::lock_guard<std::mutex> lock{ mutex };
			for (auto& it : pressed_buttons_)
			{
				if (it.second)
				{
					Native::GetInstance()->SendKeysUp((uint32_t*)&it.first, 1);
				}
			}
			pressed_buttons_.clear();
			stopped_ = false;
		}
	}

	void OnChange(int index, const InputStatus& status) override
	{
		std::lock_guard<std::mutex> lock{ mutex };
		if (stopped_)
			return;

		if (status.reset)
		{
			Native::GetInstance()->SendKeysUp((uint32_t*)&index, 1);
		}
		else
		{
			Native::GetInstance()->SendKeysDown((uint32_t*)&index, 1);
		}
		pressed_buttons_[index] = status.reset;
	}

	void OnStop() override
	{
		stopped_ = true;
	}

private:
	bool stopped_{};
	std::unordered_map<uint32_t, bool> pressed_buttons_;
	mutable std::mutex mutex;
};

NpadController::NpadController()
	: input_handlers_({
		new StickInputHandler(),
		new ButtonInputHandler()
		})
{
	update_thread = std::jthread([this](std::stop_token stop_token) { UpdateThread(stop_token); });
}

void NpadController::UpdateThread(std::stop_token stop_token)
{
	while (!stop_token.stop_requested())
	{
		for (auto& handler : input_handlers_)
		{
			handler->OnUpdate();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ClearState();
	fprintf(stdout, "Exiting Controller.\n");
}

void NpadController::SetStick(float raw_x, float raw_y)
{
	std::lock_guard<std::mutex> lock{ mutex };
	if (last_raw_x_ != raw_x || last_raw_y_ != raw_y)
	{
#if _DEBUG
		fprintf(stdout, "new change: %f, %f\n", raw_x, raw_y);
#endif
		last_raw_x_ = raw_x;
		last_raw_y_ = raw_y;
		OnChange();
	}
}

void NpadController::SetButton(int button, int value)
{
	input_handlers_[ButtonInputHandlerIndex]->OnChange(button, { value == 0, 0 });
}

void NpadController::OnChange()
{
	const auto camera_update = Config::Current()->CAMERA_UPDATE_TIME;
	SanatizeAxes(last_raw_x_, last_raw_y_, true);

	auto last_x = axes_.x, last_y = axes_.y;

	axes_.x = static_cast<int>(last_x_ * camera_update);
	axes_.y = static_cast<int>(last_y_ * camera_update);

	InputStatus x_status{ axes_.x == 0, (axes_.x == 0 ? last_x : axes_.x) };
	InputStatus y_status{ axes_.y == 0, (axes_.y == 0 ? last_y : axes_.y) };

	input_handlers_[StickInputHandlerIndex]->OnChange(x_axis, x_status);
	input_handlers_[StickInputHandlerIndex]->OnChange(y_axis, y_status);

	axes_.right = last_x_ > threshold;
	axes_.left = last_x_ < -threshold;
	axes_.up = last_y_ > threshold;
	axes_.down = last_y_ < -threshold;

#if _DEBUG
	if (axes_.x != 0 || axes_.y != 0)
	{
		fprintf(stdout, "axes_change: %d, %d - actual_value: %f, %f\n", axes_.x, axes_.y, last_x_, last_y_);
		fprintf(stdout, "right: %d left: %d up: %d down: %d\n", axes_.right, axes_.left, axes_.up, axes_.down);
	}
#endif
}

void NpadController::ClearState()
{
	last_raw_x_ = last_raw_y_ = last_x_ = last_y_ = 0.f;
	axes_ = {};

	for (auto& input_device : input_handlers_)
	{
		input_device->OnStop();
	}
}

void NpadController::SanatizeAxes(float raw_x, float raw_y, bool clamp_value)
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
