#pragma once
#include <thread>
#include <mutex>
#include <vector>

constexpr int BUTTONS = 4;

struct Axes
{
	float x;
	float y;

	bool left;
	bool right;
	bool up;
	bool down;
};

struct InputStatus
{
	bool reset;
	float value;
};

class InputDevice;
class ButtonInputDevice;
class StickInputDevice;

class NpadController
{
public:
	NpadController();

	void SetStick(float raw_x, float raw_y);
	void SetButton(int button, int value);
	void ClearState();

private:

	void UpdateThread(std::stop_token stop_token);
	void OnChange();
	void SanatizeAxes(float raw_x, float raw_y, bool clamp_value);

	std::vector<InputDevice*> input_devices_;

	float last_raw_x_;
	float last_raw_y_;

	float last_x_;
	float last_y_;

	Axes axes_{};

	std::jthread update_thread;
	mutable std::mutex mutex;
};