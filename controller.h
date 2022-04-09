#pragma once
#include <mutex>

constexpr int BUTTONS = 4;

class Controller
{
public:
	void Update();
	void SetAxis(float raw_x, float raw_y);

private:

	struct Axes
	{
		int x;
		int y;

		bool left;
		bool right;
		bool up;
		bool down;
	};
	void OnChange();
	void SanatizeAxes(float raw_x, float raw_y, bool clamp_value);

	long timeouts_[BUTTONS]{};
	bool sent_axis_buttons_[BUTTONS]{};

	float last_raw_x_;
	float last_raw_y_;

	float last_x_;
	float last_y_;

	Axes axes_{};
	Axes sent_axes_{};

	mutable std::mutex mutex;
};