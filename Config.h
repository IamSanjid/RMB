#pragma once
#include <cstdint>

class Config
{
public:
	static Config* New();
	static Config* Default();
	static Config* Current(Config* change = nullptr);

	const char* NAME = "RMB";
	const char* TARGET_NAME = "Ryujinx";
	const uint32_t WIDTH = 420;
	const uint32_t HEIGHT = 500;

	uint32_t TOGGLE_MODIFIER;
	uint32_t TOGGLE_KEY;

	uint32_t* RIGHT_STICK_KEYS;
	uint32_t LEFT_MOUSE_KEY;
	uint32_t RIGHT_MOUSE_KEY;
	uint32_t MIDDLE_MOUSE_KEY;

	float CAMERA_UPDATE_TIME;
	float SENSITIVITY;

	bool HIDE_MOUSE;
	bool AUTO_FOCUS_RYU;
	bool BIND_MOUSE_BUTTON;

private:
	Config();
	~Config();

	static Config* default_instance_;
};
