#pragma once
#include <mutex>
#include <vector>

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
	int value;
};

class Device;
class AnalogDevice;
class AnalogStickDevice;

class Controller
{
public:
	Controller();

	void Update();
	void StopDevice();
	void SetAxis(float raw_x, float raw_y);
	void SetButton(int button, int value);

private:
	std::vector<Device*> devices;

	void OnChange();
	void SanatizeAxes(float raw_x, float raw_y, bool clamp_value);

	float last_raw_x_;
	float last_raw_y_;

	float last_x_;
	float last_y_;

	Axes axes_{};

	mutable std::mutex mutex;
};