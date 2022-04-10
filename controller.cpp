#include "controller.h"
#include <stdio.h>
#include <stdint.h>
#include <cmath>

#include "native.h"
#include "Config.h"
#include "Utils.h"
#include "Application.h"

/* TODO: make these configurable */
const float deadzone = 0.09f;
const float range = 1.0f;
const float threshold = 0.5f;
const float offset = 0.0f;

constexpr int x_axis = 0;
constexpr int y_axis = 1;

class Device
{
public:
	virtual void OnUpdate() = 0;
	virtual void OnChange(int index, const DeviceStatus& state) = 0;
	virtual void OnStop() = 0;
};

class AnalogDevice final : public Device
{
public:
	void OnUpdate() override
	{
		double current_time = Application::GetTotalRunningTime();
		double time_passed = current_time - last_update_;

		last_update_ = current_time;

		if (stopped_)
		{
			for (int i = 0; i < BUTTONS; i++)
			{
				if (timeouts_[i] > 0.0)
				{
					Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[i], 1);
				}
			}
			stopped_ = false;
			return;
		}

		double start_wasted_time = Application::GetTotalRunningTime();
		std::lock_guard<std::mutex> lock{ mutex };

		if (time_passed != last_update_)
		{
			for (int i = 0; i < BUTTONS; i++)
			{
				if (timeouts_[i] > 0.0)
				{
					if (time_passed >= timeouts_[i])
					{
						Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[i], 1);
						timeouts_[i] = 0.0;
						continue;
					}
					timeouts_[i] -= time_passed;
				}
			}
		}

		double end_wasted_time = Application::GetTotalRunningTime();
		last_update_ += end_wasted_time - start_wasted_time;
	}

	void OnChange(int index, const DeviceStatus& status) override
	{
		std::lock_guard<std::mutex> lock{ mutex };

		int button = (index * 2) + (Utils::sign(status.value) == 1);
		int opposite_button = (index * 2) + (Utils::sign(status.value) != 1);
		double time = status.reset ? 0.0 : std::abs(status.value);

		if (time == 0.0 && timeouts_[button] > 0.0)
		{
			timeouts_[button] = 0.0;
			Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[button], 1);
		}
		else if (time > 0.0)
		{
			if (timeouts_[button] == 0.0)
			{
				Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[button], 1);
			}
			if (timeouts_[opposite_button] > 0.0)
			{
				timeouts_[opposite_button] = 0.1;
			}
			timeouts_[button] = time;
		}
	}

	void OnStop() override
	{
		stopped_ = true;
	}

private:
	double timeouts_[BUTTONS]{};
	double last_update_{};
	bool stopped_{};

	mutable std::mutex mutex;
};

Controller::Controller()
	: device(new AnalogDevice())
{
}

void Controller::Update()
{
	device->OnUpdate();
}

void Controller::StopDevice()
{
	device->OnStop();
}

void Controller::SetAxis(float raw_x, float raw_y)
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

void Controller::OnChange()
{
	const auto camera_update = Config::Current()->CAMERA_UPDATE_TIME;
	SanatizeAxes(last_raw_x_, last_raw_y_, true);

	int last_x = axes_.x, last_y = axes_.y;

	axes_.x = static_cast<int32_t>(last_x_ * camera_update);
	axes_.y = static_cast<int32_t>(last_y_ * camera_update);

	DeviceStatus x_status{ axes_.x == 0, static_cast<double>(axes_.x == 0 ? last_x : axes_.x) };
	DeviceStatus y_status{ axes_.y == 0, static_cast<double>(axes_.y == 0 ? last_y : axes_.y) };

	device->OnChange(x_axis, x_status);
	device->OnChange(y_axis, y_status);

	axes_.right = last_x_ > threshold;
	axes_.left = last_x_ < -threshold;
	axes_.up = last_y_ > threshold;
	axes_.down = last_y_ < -threshold;

#if _DEBUG
	fprintf(stdout, "axes_change: %d, %d - actual_value: %f, %f\n", axes_.x, axes_.y, last_x_, last_y_);
	fprintf(stdout, "right: %d left: %d up: %d down: %d\n", axes_.right, axes_.left, axes_.up, axes_.down);
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
