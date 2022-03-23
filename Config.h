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

	uint32_t TOGGLE_MODIFIER;
	uint32_t TOGGLE_KEY;

	uint32_t* RIGHT_STICK_KEYS;

	double CAMERA_UPDATE_TIME;
	double SENSITIVITY;

	bool HIDE_MOUSE;
	bool AUTO_FOCUS_RYU;

private:
	Config();

	static Config* default_instance_;
};
