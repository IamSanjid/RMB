#include "Config.h"
#include <GLFW/glfw3.h>

Config* Config::default_instance_ = nullptr;

Config::Config()
{
	TOGGLE_MODIFIER = GLFW_KEY_LEFT_CONTROL;
	TOGGLE_KEY = GLFW_KEY_F9;

	/* default ryujinx right stick keys j = left, l = right, i = up, k = down */
	RIGHT_STICK_KEYS = new uint32_t[4]
	{
		GLFW_KEY_J, GLFW_KEY_L, GLFW_KEY_I, GLFW_KEY_K
	};

	CAMERA_UPDATE_TIME = 60.0f;
	SENSITIVITY = 10.0f;

	HIDE_MOUSE = true;
	AUTO_FOCUS_RYU = true;
}

Config* Config::Default()
{
	if (!default_instance_)
		default_instance_ = new Config();
	return default_instance_;
}

Config* Config::Current(Config* change)
{
	static Config* current_instance_ = new Config();
	if (!default_instance_)
		default_instance_ = new Config();
	if (change)
	{
		current_instance_ = change;
		default_instance_ = new Config();
	}
	return current_instance_;
}
