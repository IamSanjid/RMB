#pragma once
#include <cstdint>

class Config
{
public:

	static Config* Default();
	static Config* Current(Config* change = nullptr);

	const char* NAME = "RMB";
	const uint32_t WIDTH = 600;
	const uint32_t HEIGHT = 400;

	uint32_t TOGGLE_MODIFIER = 341; // GLFW_KEY_LEFT_CONTROL
	uint32_t TOGGLE_KEY = 298; // GLFW_KEY_F9

	uint32_t* RIGHT_STICK_KEYS;

	double CAMERA_UPDATE_TIME = 960.0;
	double SENSITIVITY = 10.0;

	bool HIDE_MOUSE = true;
	bool AUTO_FOCUS_RYU = true;

private:
	Config();

	static Config* default_instance_;
};
