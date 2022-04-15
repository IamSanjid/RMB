#include "MainView.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "../Application.h"
#include "../Config.h"
#include "../native.h"
#include "../Utils/iniparser.hpp"

const char* INI_FILE = "RMB.ini";

MainView::MainView()
{
	IMGUI_CHECKVERSION();
	name_ = "Main";

	glfw_modifiers = std::vector<uint32_t>
	{
		(uint32_t)glfwGetKeyScancode(GLFW_KEY_LEFT_CONTROL),
		(uint32_t)glfwGetKeyScancode(GLFW_KEY_LEFT_SHIFT),
		(uint32_t)glfwGetKeyScancode(GLFW_KEY_LEFT_ALT)
	};

	scancodes_to_glfw_[glfw_modifiers[0]] = GLFW_KEY_LEFT_CONTROL;
	scancodes_to_glfw_[glfw_modifiers[1]] = GLFW_KEY_LEFT_SHIFT;
	scancodes_to_glfw_[glfw_modifiers[2]] = GLFW_KEY_LEFT_ALT;

	for (int f_key = GLFW_KEY_F1; f_key <= GLFW_KEY_F25; f_key++)
	{
		int scan_code = glfwGetKeyScancode(f_key);
		if (scan_code)
		{
			std::string str = ("F" + std::to_string(f_key + 1 - GLFW_KEY_F1));
			char* new_str = new char[str.length() + 1]{ 0 };
#ifdef _MSC_VER
			strcpy_s(new_str, str.length() + 1, str.c_str());
#else
			strcpy(new_str, str.c_str());
#endif
			glfw_str_keys.push_back(new_str);
			glfw_keys.push_back(scan_code);
			scancodes_to_glfw_[scan_code] = f_key;
		}
	}

	glfw_str_keys.push_back("SPACE");
	glfw_keys.push_back(glfwGetKeyScancode(GLFW_KEY_SPACE));

	for (auto key = GLFW_KEY_COMMA; key <= GLFW_KEY_GRAVE_ACCENT; key++)
	{
		int scan_code = glfwGetKeyScancode(key);
		if (scan_code > 0)
		{
			auto str = glfwGetKeyName(key, scan_code);
			if (str)
			{
				glfw_str_keys.push_back(str);
				glfw_keys.push_back(scan_code);
			}
			scancodes_to_glfw_[scan_code] = key;
		}
	}
	/* get the default toggle keys */
	GetToggleKeys();

	ReadConfig();
	GetToggleKeys();
	GetRightStickButtons();
	GetMouseButtons();
}

MainView::~MainView()
{
	SaveConfig();
}

void MainView::Show()
{
	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);

	ImGui::Begin(name_, 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

	if (ImGui::Button(Application::GetInstance()->IsPanning() ? "Stop" : "Start"))
	{
		Application::GetInstance()->TogglePanning();
	}

	/* panning toggle hotkey changer.. */
	ImGui::TextDisabled("Panning Toggle Hotkeys");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Choose this carefully. Make sure no other application is\nusing the same hotkey.");
	ImGui::Text("Modifier: ");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##modifier_combo", modifiers[modifier_selected]))
	{
		for (int n = 0; n < IM_ARRAYSIZE(modifiers); n++)
		{
			bool is_selected = modifier_selected == n;
			if (ImGui::Selectable(modifiers[n], is_selected))
				modifier_selected = n;
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (Config::Current()->TOGGLE_MODIFIER != glfw_modifiers[modifier_selected])
	{
		Native::GetInstance()->UnregisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
		Config::Current()->TOGGLE_MODIFIER = glfw_modifiers[modifier_selected];
		Native::GetInstance()->RegisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
	}

	ImGui::Text("Key:      ");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##key_combo", glfw_str_keys[key_selected]))
	{
		for (int n = 0; n < (int)glfw_str_keys.size(); n++)
		{
			bool is_selected = key_selected == n;
			if (ImGui::Selectable(glfw_str_keys[n], is_selected))
				key_selected = n;
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (Config::Current()->TOGGLE_KEY != glfw_keys[key_selected])
	{
		Native::GetInstance()->UnregisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
		Config::Current()->TOGGLE_KEY = glfw_keys[key_selected];
		Native::GetInstance()->RegisterHotKey(Config::Current()->TOGGLE_KEY, Config::Current()->TOGGLE_MODIFIER);
	}

	ImGui::NewLine();

	ImGui::Checkbox("Hide Mouse in Panning Mode", &Config::Current()->HIDE_MOUSE);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Hides the normal mouse cursor.");
	ImGui::Checkbox("Auto Focus Ryujinx", &Config::Current()->AUTO_FOCUS_RYU);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Tries to focus Ryujinx after enabling mouse panning.");
	ImGui::Checkbox("Bind Mouse Buttons", &Config::Current()->BIND_MOUSE_BUTTON);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Bind keyboard keys to mouse buttons.");

	ImGui::Text("Sensitivity(%%):");
	if (ImGui::InputFloat(" ", &Config::Current()->SENSITIVITY, 0.5f, 0.0f, "%0.3f"))
	{
		if (Config::Current()->SENSITIVITY < 1.f)
			Config::Current()->SENSITIVITY = 1.f;
		else if (Config::Current()->SENSITIVITY > 100.f)
			Config::Current()->SENSITIVITY = 100.f;
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Camera sensitivity. The higher the faster the camera\nview will be changed.");

	ImGui::Text("Camera Update Time(%%):");
	if (ImGui::InputFloat(" ", &Config::Current()->CAMERA_UPDATE_TIME, 0.5f, 5.0f, "%0.3f"))
	{
		if (Config::Current()->CAMERA_UPDATE_TIME < 10.f)
			Config::Current()->CAMERA_UPDATE_TIME = 10.f;
		else if (Config::Current()->CAMERA_UPDATE_TIME > 100.f)
			Config::Current()->CAMERA_UPDATE_TIME = 100.f;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Base camera update time, the lower the slower it will\nupdate the camera. Kind of like sensitivity but not\nrecommended to change.\nBut you may need to change it for different games.");

	ImGui::NewLine();

	/* configure input keys */
	auto window_size = ImGui::GetWindowSize();
	float width = window_size.x * 0.5f;
	float height = window_size.y * 0.375f;
	float delta_width = width * 0.5f;

	ImGui::Button("Configure Input");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Configure Keyboard Keys to match with your Ryujinx\nconfiguration and bind mouse buttons with other\nkeyboard keys.");

	ImGui::SameLine();

	if (ImGui::Button("Default"))
	{
		selected_keys_.clear();
		modifier_selected = 0; key_selected = 8;
		Application::GetInstance()->Reconfig();
		GetRightStickButtons();
		GetMouseButtons();
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Resets to default configuration.");

	ImGui::BeginChild("conf_input", ImVec2(width, height), true, ImGuiWindowFlags_HorizontalScrollbar);

	ImGui::Text("Stick Keys:");

	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Make sure you select the correct up/down/left/right keys\nwhich are bound with your keyboard.");
	}

	bool bounded = false;

	for (int i = 2; i < 4; i++)
	{
		if (i == 2 && bounded)
			continue;

		auto text_width = ImGui::CalcTextSize(r_btn_text_[i].c_str()).x;
		if (i == 2 || i == 3)
			ImGui::SetCursorPosX((delta_width - text_width) * 0.5f);
		else if (i == 1)
			ImGui::SameLine((delta_width - text_width) * 0.9072f);

		bool other_btn_changing = any_mouse_btn_changing_;

		for (int j = 0; j < 4; j++)
		{
			if (j == i)
				continue;
			if (r_btn_changing_[j])
			{
				other_btn_changing = true;
				break;
			}
		}

		if (ImGui::Button(r_btn_text_[i].c_str()) && !other_btn_changing)
		{
			r_btn_changing_[i] = !r_btn_changing_[i];
			if (r_btn_changing_[i])
			{
				selected_keys_[r_btn_key_codes_[i]] = false;
				r_btn_text_[i] = "waiting..";
				any_r_btn_chaniging_ = true;
			}
			else
			{
				selected_keys_[r_btn_key_codes_[i]] = true;
				auto str = glfwGetKeyName(r_btn_key_codes_[i], Config::Current()->RIGHT_STICK_KEYS[i]);
				if (str)
					r_btn_text_[i] = std::string(str);
				else
					r_btn_text_[i] = std::to_string(Config::Current()->RIGHT_STICK_KEYS[i]);
				any_r_btn_chaniging_ = false;
			}
		}

		if (ImGui::IsItemHovered())
		{
			if (i == 0)
				ImGui::SetTooltip("Left button");
			else if (i == 1)
				ImGui::SetTooltip("Right button");
			else if (i == 2)
				ImGui::SetTooltip("Up button");
			else if (i == 3)
				ImGui::SetTooltip("Down button");
		}

		if (i == 2)
		{
			i = -1;
			bounded = true;
		}
	}

	ImGui::Text("Bind Mouse Buttons: ");
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Bind keyboard keys with your mouse buttons.");
	}

	for (int i = 0; i < 3; i++)
	{
		int scancode = 0;
		std::string btn = "";
		std::string key_txt = "";
		std::string hover_text = "";

		bool other_btn_changing = any_r_btn_chaniging_;

		for (int j = 0; j < 3; j++)
		{
			if (j == i)
				continue;
			if (mouse_btn_changing_[j])
			{
				other_btn_changing = true;
				break;
			}
		}

		switch (i)
		{
		case 0:
			scancode = Config::Current()->LEFT_MOUSE_KEY;
			btn = "Left Mouse";
			break;
		case 1:
			scancode = Config::Current()->RIGHT_MOUSE_KEY;
			btn = "Right Mouse";
			break;
		case 2:
			scancode = Config::Current()->MIDDLE_MOUSE_KEY;
			btn = "Middle Mouse";
			break;
		}

		key_txt = btn + ": " + mouse_btn_text_[i];

		if (ImGui::Button(key_txt.c_str()) && !other_btn_changing)
		{
			mouse_btn_changing_[i] = !mouse_btn_changing_[i];
			if (mouse_btn_changing_[i])
			{
				selected_keys_[mouse_btn_key_codes_[i]] = false;
				mouse_btn_text_[i] = "waiting..";
				any_mouse_btn_changing_ = true;
			}
			else
			{
				selected_keys_[mouse_btn_key_codes_[i]] = true;
				auto str = glfwGetKeyName(mouse_btn_key_codes_[i], scancode);
				if (str)
					mouse_btn_text_[i] = std::string(str);
				else
					mouse_btn_text_[i] = std::to_string(scancode);

				if (scancode <= 0)
					mouse_btn_text_[i] = "None";

				any_mouse_btn_changing_ = false;
			}
		}

		hover_text = "The following button will be pressed if " + btn + "\nbutton is clicked.";

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(hover_text.c_str());
		}
	}

	ImGui::EndChild();
#if _DEBUG
	ImGui::InputInt("Test Delay", &test_delay, 10);
	if (test_delay < 10)
		test_delay = 10;
	ImGui::InputInt("Test Type", &test_type, 1);
	if (test_type < 0 || test_type > 2)
		test_type = 0;
#endif
	ImGui::End();
}

void MainView::OnKeyPress(int key, int scancode, int mods)
{
	(void)mods;
	if (scancode <= 0)
		return;

	if (selected_keys_[key] || scancodes_to_glfw_.find(scancode) == scancodes_to_glfw_.cend())
		return;

	for (int i = 0; i < 4; i++)
	{
		if (r_btn_changing_[i])
		{
			r_btn_changing_[i] = false;
			any_r_btn_chaniging_ = false;

			auto str = glfwGetKeyName(key, scancode);
			if (str)
				r_btn_text_[i] = std::string(str);
			else
				r_btn_text_[i] = std::to_string(key);

			r_btn_key_codes_[i] = key;
			selected_keys_[key] = true;

			Config::Current()->RIGHT_STICK_KEYS[i] = scancode;
			return;
		}
	}

	for (int i = 0; i < 3; i++)
	{
		if (mouse_btn_changing_[i])
		{
			mouse_btn_changing_[i] = false;
			any_mouse_btn_changing_ = false;

			auto str = glfwGetKeyName(key, scancode);
			if (str)
				mouse_btn_text_[i] = std::string(str);
			else
				mouse_btn_text_[i] = std::to_string(key);

			mouse_btn_key_codes_[i] = key;
			selected_keys_[key] = true;

			switch (i)
			{
			case 0:
				Config::Current()->LEFT_MOUSE_KEY = scancode;
				return;
			case 1:
				Config::Current()->RIGHT_MOUSE_KEY = scancode;
				return;
			case 2:
				Config::Current()->MIDDLE_MOUSE_KEY = scancode;
				return;
			}
		}
	}
}

void MainView::GetToggleKeys()
{
	auto current_modifier = Config::Current()->TOGGLE_MODIFIER;
	auto current_key = Config::Current()->TOGGLE_KEY;
	for (int i = 0; i < (int)glfw_modifiers.size(); i++)
	{
		if (glfw_modifiers[i] == current_modifier)
		{
			modifier_selected = i;
			break;
		}
	}
	for (int i = 0; i < (int)glfw_keys.size(); i++)
	{
		if (glfw_keys[i] == current_key)
		{
			key_selected = i;
			break;
		}
	}
}

void MainView::GetRightStickButtons()
{
	for (auto& it : scancodes_to_glfw_)
	{
		uint32_t scan_code = it.first;
		int key = it.second;
		for (auto i = 0; i < 4; i++)
		{
			if (Config::Current()->RIGHT_STICK_KEYS[i] == scan_code)
			{
				auto str = glfwGetKeyName(key, scan_code);
				r_btn_key_codes_[i] = key;
				selected_keys_[r_btn_key_codes_[i]] = true;
				if (str)
					r_btn_text_[i] = std::string(str);
				else
					r_btn_text_[i] = std::to_string(key);
			}
		}
	}
}

void MainView::GetMouseButtons()
{
	for (int i = 0; i < 3; i++)
	{
		int scancode = 0;
		switch (i)
		{
		case 0:
			scancode = Config::Current()->LEFT_MOUSE_KEY;
			break;
		case 1:
			scancode = Config::Current()->RIGHT_MOUSE_KEY;
			break;
		case 2:
			scancode = Config::Current()->MIDDLE_MOUSE_KEY;
			break;
		}

		if (scancode <= 0)
		{
			mouse_btn_text_[i] = "None";
			mouse_btn_key_codes_[i] = 0;
		}
		else
		{
			auto str = glfwGetKeyName(mouse_btn_key_codes_[i], scancode);
			if (str)
				mouse_btn_text_[i] = std::string(str);
			else
				mouse_btn_text_[i] = std::to_string(mouse_btn_key_codes_[i]);
		}
	}
}

void MainView::SaveConfig()
{
	INI::File ft;
	ft.Load(INI_FILE);

	ft.SetValue("Sensitivity", Config::Current()->SENSITIVITY);
	ft.SetValue("CameraUpdateTime", Config::Current()->CAMERA_UPDATE_TIME);

	ft.SetValue("HideMouse", Config::Current()->HIDE_MOUSE);
	ft.SetValue("AutoFocusRyujinx", Config::Current()->AUTO_FOCUS_RYU);
	ft.SetValue("BindMouseButton", Config::Current()->BIND_MOUSE_BUTTON);

	for (int i = 0; i < 4; i++)
	{
		std::string key = "RightStick:" + std::to_string(i);
		ft.SetValue(key, r_btn_key_codes_[i]);
	}

	ft.SetValue("Mouse:LeftButton", mouse_btn_key_codes_[0]);
	ft.SetValue("Mouse:RightButton", mouse_btn_key_codes_[1]);
	ft.SetValue("Mouse:MiddleButton", mouse_btn_key_codes_[2]);

	ft.SetValue("PanningToggle:Modifier", scancodes_to_glfw_[glfw_modifiers[modifier_selected]]);
	ft.SetValue("PanningToggle:Key", scancodes_to_glfw_[glfw_keys[key_selected]]);

	ft.Save(INI_FILE);
}

void MainView::ReadConfig()
{
	INI::File ft;
	if (!ft.Load(INI_FILE))
		return;

	auto new_conf = Config::New();

	new_conf->SENSITIVITY = static_cast<float>(ft.GetValue("Sensitivity", Config::Current()->SENSITIVITY).AsDouble());
	new_conf->CAMERA_UPDATE_TIME = static_cast<float>(ft.GetValue("CameraUpdateTime", Config::Current()->CAMERA_UPDATE_TIME).AsDouble());

	new_conf->HIDE_MOUSE = ft.GetValue("HideMouse", Config::Current()->HIDE_MOUSE).AsBool();
	new_conf->AUTO_FOCUS_RYU = ft.GetValue("AutoFocusRyujinx", Config::Current()->AUTO_FOCUS_RYU).AsBool();
	new_conf->BIND_MOUSE_BUTTON = ft.GetValue("BindMouseButton", Config::Current()->BIND_MOUSE_BUTTON).AsBool();

	for (int i = 0; i < 4; i++)
	{
		std::string key = "RightStick:" + std::to_string(i);
		new_conf->RIGHT_STICK_KEYS[i] = ft.GetValue(key, r_btn_key_codes_[i]).AsInt();
		r_btn_key_codes_[i] = new_conf->RIGHT_STICK_KEYS[i];
	}

	new_conf->LEFT_MOUSE_KEY = mouse_btn_key_codes_[0] = ft.GetValue("Mouse:LeftButton", mouse_btn_key_codes_[0]).AsInt();
	new_conf->RIGHT_MOUSE_KEY = mouse_btn_key_codes_[1] = ft.GetValue("Mouse:RightButton", mouse_btn_key_codes_[1]).AsInt();
	new_conf->MIDDLE_MOUSE_KEY = mouse_btn_key_codes_[2] = ft.GetValue("Mouse:MiddleButton", mouse_btn_key_codes_[2]).AsInt();

	new_conf->TOGGLE_MODIFIER = ft.GetValue("PanningToggle:Modifier", scancodes_to_glfw_[glfw_modifiers[modifier_selected]]).AsInt();
	new_conf->TOGGLE_KEY = ft.GetValue("PanningToggle:Key", scancodes_to_glfw_[glfw_keys[key_selected]]).AsInt();

	Application::GetInstance()->Reconfig(new_conf);
}
