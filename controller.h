#pragma once
#include <mutex>

constexpr int BUTTONS = 4;

struct Axes
{
	int x;
	int y;

	bool left;
	bool right;
	bool up;
	bool down;
};

struct DeviceStatus
{
	bool reset;
	double value;
};

struct State
{

};

class Device;
class AnalogDevice;
class Controller
{
public:
	Controller();

	void Update();
	void StopDevice();
	void SetAxis(float raw_x, float raw_y);

private:
	Device* device;

	void OnChange();
	void SanatizeAxes(float raw_x, float raw_y, bool clamp_value);

	float last_raw_x_;
	float last_raw_y_;

	float last_x_;
	float last_y_;

	Axes axes_{};

	mutable std::mutex mutex;
};