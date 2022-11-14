#pragma once
#include <cstdint>
#include <string>

class Config
{
public:
	static Config* New();
	static Config* Default();
	static Config* Current(Config* change = nullptr);
	static Config* LoadNew(const std::string& file);

	void Save(const std::string& file);

	const char* NAME = "RMB";
	const uint32_t WIDTH = 420;
	const uint32_t HEIGHT = 540;

	std::string TARGET_NAME = "Ryujinx";

	uint32_t TOGGLE_MODIFIER;
	uint32_t TOGGLE_KEY;

	uint32_t* RIGHT_STICK_KEYS;
	uint32_t LEFT_MOUSE_KEY;
	uint32_t RIGHT_MOUSE_KEY;
	uint32_t MIDDLE_MOUSE_KEY;

	float CAMERA_UPDATE_TIME;
	float SENSITIVITY;

	bool HIDE_MOUSE;
	bool AUTO_FOCUS_EMU_WINDOW;
	bool BIND_MOUSE_BUTTON;

private:
	Config();
	~Config();

	static Config* default_instance_;
};
