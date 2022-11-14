#pragma once
#include <thread>
#include <mutex>
#include <vector>

struct Axes
{
	int x;
	int y;

	bool left;
	bool right;
	bool up;
	bool down;
};

struct InputStatus
{
	bool reset;
	int value;
};

class InputHandler;
class ButtonInputHandler;
class StickInputHandler;

class NpadController
{
public:
	NpadController();

	void SetStick(float raw_x, float raw_y);
	void SetButton(int button, int value);
	void ClearState();

private:
	void UpdateThread(std::stop_token stop_token);
	void SanatizeAxes(float raw_x, float raw_y, bool clamp_value);

	std::vector<InputHandler*> input_handlers_;

	float last_raw_x_;
	float last_raw_y_;

	float last_x_;
	float last_y_;

	Axes axes_{};

	std::jthread update_thread;
	mutable std::mutex mutex;
};