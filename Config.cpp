#include "Config.h"
#include <GLFW/glfw3.h>
#include <iniparser.hpp>

Config* Config::default_instance_ = nullptr;

Config::Config()
{
	constexpr uint32_t NONE = 0x0;

	TOGGLE_MODIFIER = GLFW_KEY_LEFT_CONTROL;
	TOGGLE_KEY = GLFW_KEY_F9;

	/* default ryujinx right stick keys j = left, l = right, i = up, k = down */
	RIGHT_STICK_KEYS = new uint32_t[4]
	{
		GLFW_KEY_J, GLFW_KEY_L, GLFW_KEY_I, GLFW_KEY_K
	};
	LEFT_MOUSE_KEY = NONE;
	RIGHT_MOUSE_KEY = NONE;
	MIDDLE_MOUSE_KEY = NONE;

	CAMERA_UPDATE_TIME = 273.0f;
	SENSITIVITY = 10.0f;

	HIDE_MOUSE = true;
	AUTO_FOCUS_EMU_WINDOW = true;
	BIND_MOUSE_BUTTON = true;
	SPECIAL_ROUNDING = false;
}

Config::~Config()
{
	delete[] RIGHT_STICK_KEYS;
}

Config* Config::New()
{
	return new Config();
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

Config* Config::LoadNew(const std::string& file)
{
	INI::File ft;
	if (!ft.Load(file))
		return nullptr;

	auto current_conf = Config::Current();
	auto new_conf = Config::New();

	new_conf->TARGET_NAME = ft.GetValue("TargetEmulatorWindow", current_conf->TARGET_NAME).AsString();

	new_conf->SENSITIVITY = ft.GetValue("Sensitivity", current_conf->SENSITIVITY).AsT<float>();
	new_conf->CAMERA_UPDATE_TIME = ft.GetValue("CameraUpdateTime", current_conf->CAMERA_UPDATE_TIME).AsT<float>();

	new_conf->HIDE_MOUSE = ft.GetValue("HideMouse", current_conf->HIDE_MOUSE).AsBool();
	new_conf->AUTO_FOCUS_EMU_WINDOW = ft.GetValue("AutoFocusEmuWindow", current_conf->AUTO_FOCUS_EMU_WINDOW).AsBool();
	new_conf->BIND_MOUSE_BUTTON = ft.GetValue("BindMouseButton", current_conf->BIND_MOUSE_BUTTON).AsBool();
	new_conf->SPECIAL_ROUNDING = ft.GetValue("SpecialRounding", current_conf->SPECIAL_ROUNDING).AsBool();

	new_conf->DEADZONE = ft.GetValue("AnalogProperties:DeadZone", current_conf->DEADZONE).AsT<float>();
	new_conf->RANGE = ft.GetValue("AnalogProperties:Range", current_conf->RANGE).AsT<float>();
	new_conf->THRESHOLD = ft.GetValue("AnalogProperties:Threshold", current_conf->THRESHOLD).AsT<float>();
	new_conf->X_OFFSET = ft.GetValue("AnalogProperties:XOffset", current_conf->X_OFFSET).AsT<float>();
	new_conf->Y_OFFSET = ft.GetValue("AnalogProperties:YOffset", current_conf->Y_OFFSET).AsT<float>();

	for (int i = 0; i < 4; i++)
	{
		std::string key = "RightStick:" + std::to_string(i);
		new_conf->RIGHT_STICK_KEYS[i] = ft.GetValue(key, current_conf->RIGHT_STICK_KEYS[i]).AsInt();
	}

	new_conf->LEFT_MOUSE_KEY = ft.GetValue("Mouse:LeftButton", current_conf->LEFT_MOUSE_KEY).AsInt();
	new_conf->RIGHT_MOUSE_KEY = ft.GetValue("Mouse:RightButton", current_conf->RIGHT_MOUSE_KEY).AsInt();
	new_conf->MIDDLE_MOUSE_KEY = ft.GetValue("Mouse:MiddleButton", current_conf->MIDDLE_MOUSE_KEY).AsInt();

	new_conf->TOGGLE_MODIFIER = ft.GetValue("PanningToggle:Modifier", current_conf->TOGGLE_MODIFIER).AsInt();
	new_conf->TOGGLE_KEY = ft.GetValue("PanningToggle:Key", current_conf->TOGGLE_KEY).AsInt();

	return new_conf;
}

void Config::Save(const std::string& file)
{
	INI::File ft;
	ft.Load(file);

	ft.SetValue("TargetEmulatorWindow", this->TARGET_NAME);

	ft.SetValue("Sensitivity", this->SENSITIVITY);
	ft.SetValue("CameraUpdateTime", this->CAMERA_UPDATE_TIME);

	ft.SetValue("HideMouse", this->HIDE_MOUSE);
	ft.SetValue("AutoFocusEmuWindow", this->AUTO_FOCUS_EMU_WINDOW);
	ft.SetValue("BindMouseButton", this->BIND_MOUSE_BUTTON);
	ft.SetValue("SpecialRounding", this->SPECIAL_ROUNDING);

	ft.SetValue("AnalogProperties:DeadZone", this->DEADZONE);
	ft.SetValue("AnalogProperties:Range", this->RANGE);
	ft.SetValue("AnalogProperties:Threshold", this->THRESHOLD);
	ft.SetValue("AnalogProperties:XOffset", this->X_OFFSET);
	ft.SetValue("AnalogProperties:YOffset", this->Y_OFFSET);

	for (int i = 0; i < 4; i++)
	{
		std::string key = "RightStick:" + std::to_string(i);
		ft.SetValue(key, this->RIGHT_STICK_KEYS[i]);
	}

	ft.SetValue("Mouse:LeftButton", this->LEFT_MOUSE_KEY);
	ft.SetValue("Mouse:RightButton", this->RIGHT_MOUSE_KEY);
	ft.SetValue("Mouse:MiddleButton", this->MIDDLE_MOUSE_KEY);

	ft.SetValue("PanningToggle:Modifier", this->TOGGLE_MODIFIER);
	ft.SetValue("PanningToggle:Key", this->TOGGLE_KEY);

	ft.Save(file);
}
