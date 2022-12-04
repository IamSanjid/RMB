#include "MainView.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "../Application.h"
#include "../Config.h"
#include "../native.h"

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
			glfw_str_keys.push_back(str);
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


static bool STDInputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0)
{
	struct InputTextCallback_UserData
	{
		std::string* Str;

		static int InputTextCallback(ImGuiInputTextCallbackData* data)
		{
			InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
			// Resize string callback
			// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
			std::string* str = user_data->Str;
			IM_ASSERT(data->Buf == str->c_str());
			str->resize(data->BufTextLen);
			data->Buf = (char*)str->c_str();
			return 0;
		}
	};

	IM_ASSERT((flags& ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data{};
	cb_user_data.Str = str;
	return ImGui::InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback_UserData::InputTextCallback, &cb_user_data);
}

void MainView::Show()
{
	ImGuiIO& io = ImGui::GetIO();
	auto current_config = Config::Current();

	io.IniFilename = NULL;

	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y), ImGuiCond_Always);

	ImGui::Begin(name_, 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

	if (ImGui::Button(Application::GetInstance()->IsPanning() ? "Stop" : "Start"))
	{
		Application::GetInstance()->TogglePanning();
	}

	ImGui::Text("Target Window:");
	STDInputText("##target_window", &current_config->TARGET_NAME);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("The target Emulator Window name.");

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

	if (current_config->TOGGLE_MODIFIER != glfw_modifiers[modifier_selected])
	{
		Native::GetInstance()->UnregisterHotKey(current_config->TOGGLE_KEY, current_config->TOGGLE_MODIFIER);
		current_config->TOGGLE_MODIFIER = glfw_modifiers[modifier_selected];
		Native::GetInstance()->RegisterHotKey(current_config->TOGGLE_KEY, current_config->TOGGLE_MODIFIER);
	}

	ImGui::Text("Key:      ");
	ImGui::SameLine();
	if (ImGui::BeginCombo("##key_combo", glfw_str_keys[key_selected].data()))
	{
		for (int n = 0; n < (int)glfw_str_keys.size(); n++)
		{
			bool is_selected = key_selected == n;
			if (ImGui::Selectable(glfw_str_keys[n].data(), is_selected))
				key_selected = n;
			if (is_selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (current_config->TOGGLE_KEY != glfw_keys[key_selected])
	{
		Native::GetInstance()->UnregisterHotKey(current_config->TOGGLE_KEY, current_config->TOGGLE_MODIFIER);
		current_config->TOGGLE_KEY = glfw_keys[key_selected];
		Native::GetInstance()->RegisterHotKey(current_config->TOGGLE_KEY, current_config->TOGGLE_MODIFIER);
	}

	ImGui::NewLine();

	ImGui::Checkbox("Hide Mouse on inactivity", &current_config->HIDE_MOUSE);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Hides the normal mouse cursor on inactivity.");
	ImGui::Checkbox("Auto Focus Emulator Window", &current_config->AUTO_FOCUS_EMU_WINDOW);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Tries to focus the Emulator Window after enabling mouse panning.");
	ImGui::Checkbox("Bind Mouse Buttons", &current_config->BIND_MOUSE_BUTTON);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Enable binding mouse buttons to keyboard keys.");
	ImGui::Checkbox("Special Rounding", &current_config->SPECIAL_ROUNDING);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Does some special rounding for the axis changes\nmight improve the user experience for low fps games.");

	ImGui::Text("Sensitivity(%%):");
	if (ImGui::InputFloat("##sensitivity", &current_config->SENSITIVITY, 0.5f, 3.5f, "%0.3f"))
	{
		if (current_config->SENSITIVITY < 1.f)
			current_config->SENSITIVITY = 1.f;
		else if (current_config->SENSITIVITY > 100.f)
			current_config->SENSITIVITY = 100.f;
	}

	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Camera sensitivity. The higher the faster the camera\nview will be changed.");

	ImGui::Text("Camera Update Time:");
	if (ImGui::InputFloat("##camera_update", &current_config->CAMERA_UPDATE_TIME, 1.0f, 5.0f, "%0.3f"))
	{
		if (current_config->CAMERA_UPDATE_TIME < 10.f)
			current_config->CAMERA_UPDATE_TIME = 10.f;
		else if (current_config->CAMERA_UPDATE_TIME > 1000.f)
			current_config->CAMERA_UPDATE_TIME = 1000.f;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Base camera update time, the lower the slower it will\nupdate the camera. Kind of like sensitivity but not\nrecommended to change.\nBut you may need to change it for different games.");

	ImGui::NewLine();

	/* configure input keys */
	auto window_size = ImGui::GetWindowSize();
	float width = window_size.x * 0.47f;
	float height = window_size.y * 0.335f;
	float delta_width = width * 0.5f;

	ImGui::Button("Configure Input");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Configure Keyboard Keys to match with your Emulator\nconfiguration and bind mouse buttons with other\nkeyboard keys.");

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

	ImGui::SameLine(0.f, 26.55f);

	ImGui::Button("Analog Properties");

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
				auto str = glfwGetKeyName(r_btn_key_codes_[i], current_config->RIGHT_STICK_KEYS[i]);
				if (str)
					r_btn_text_[i] = std::string(str);
				else
					r_btn_text_[i] = std::to_string(current_config->RIGHT_STICK_KEYS[i]);
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
			scancode = current_config->LEFT_MOUSE_KEY;
			btn = "Left Mouse";
			break;
		case 1:
			scancode = current_config->RIGHT_MOUSE_KEY;
			btn = "Right Mouse";
			break;
		case 2:
			scancode = current_config->MIDDLE_MOUSE_KEY;
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

		hover_text = "The following key will be pressed if " + btn + "\nbutton is clicked.";

		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(hover_text.c_str());
		}
	}

	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("analog_prop", ImVec2(width, height), true, ImGuiWindowFlags_HorizontalScrollbar);

	ImGui::Text("Deadzone:");
	if (ImGui::InputFloat("##deadzone", &current_config->DEADZONE, 0.05f, 0.2f, "%0.4f"))
	{
		if (current_config->DEADZONE < 0.f)
			current_config->DEADZONE = 0.f;
		else if (current_config->DEADZONE > 1.f)
			current_config->DEADZONE = 1.f;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Deadzone for X and Y axis.");

	ImGui::Text("Range:");
	if (ImGui::InputFloat("##range", &current_config->RANGE, 0.05f, 0.2f, "%0.4f"))
	{
		if (current_config->RANGE < 0.25f)
			current_config->RANGE = 0.25f;
		else if (current_config->RANGE > 1.50f)
			current_config->RANGE = 1.50f;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Range for X and Y axis.");

	/*ImGui::Text("Threshold:");
	if (ImGui::InputFloat("##threshold", &current_config->THRESHOLD, 0.1f, 1.f, "%0.4f"))
	{
		if (current_config->DEADZONE < 0.25f)
			current_config->DEADZONE = 0.25f;
		else if (current_config->DEADZONE > 1.50f)
			current_config->DEADZONE = 1.50f;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Threshold for X and Y axis.");*/

	ImGui::Text("X Offset:");
	if (ImGui::InputFloat("##x_offset", &current_config->X_OFFSET, 0.05f, 0.2f, "%0.4f"))
	{
		if (current_config->X_OFFSET < -1.f)
			current_config->X_OFFSET = -1.f;
		else if (current_config->X_OFFSET > 1.f)
			current_config->X_OFFSET = 1.f;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Offset for X axis.");

	ImGui::Text("Y Offset:");
	if (ImGui::InputFloat("##y_offset", &current_config->Y_OFFSET, 0.05f, 0.2f, "%0.4f"))
	{
		if (current_config->Y_OFFSET < -1.f)
			current_config->Y_OFFSET = -1.f;
		else if (current_config->Y_OFFSET > 1.f)
			current_config->Y_OFFSET = 1.f;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Offset for Y axis.");

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

	if (key == GLFW_KEY_ESCAPE) 
	{
		for (int i = 0; i < 4; i++)
		{
			r_btn_changing_[i] = false;
			if (i < 3) 
			{
				mouse_btn_changing_[i] = false;
			}
		}
		any_mouse_btn_changing_ = false;
		GetRightStickButtons();
		GetMouseButtons();
		return;
	}

	if (selected_keys_[key] || scancodes_to_glfw_.find(scancode) == scancodes_to_glfw_.cend())
		return;

	auto current_config = Config::Current();
	
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

			current_config->RIGHT_STICK_KEYS[i] = scancode;
			return;
		}

		if (i < 3 && mouse_btn_changing_[i])
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
				current_config->LEFT_MOUSE_KEY = scancode;
				return;
			case 1:
				current_config->RIGHT_MOUSE_KEY = scancode;
				return;
			case 2:
				current_config->MIDDLE_MOUSE_KEY = scancode;
				return;
			}
		}
	}
}

void MainView::GetToggleKeys()
{
	auto current_config = Config::Current();

	auto current_modifier = current_config->TOGGLE_MODIFIER;
	auto current_key = current_config->TOGGLE_KEY;
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
	auto current_config = Config::Current();

	for (auto& it : scancodes_to_glfw_)
	{
		uint32_t scan_code = it.first;
		int key = it.second;
		for (auto i = 0; i < 4; i++)
		{
			if (current_config->RIGHT_STICK_KEYS[i] == scan_code)
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
	auto current_config = Config::Current();

	for (int i = 0; i < 3; i++)
	{
		int scancode = 0;
		switch (i)
		{
		case 0:
			scancode = current_config->LEFT_MOUSE_KEY;
			break;
		case 1:
			scancode = current_config->RIGHT_MOUSE_KEY;
			break;
		case 2:
			scancode = current_config->MIDDLE_MOUSE_KEY;
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
	auto current_config = Config::Current();

	for (int i = 0; i < 4; i++)
	{
		current_config->RIGHT_STICK_KEYS[i] = r_btn_key_codes_[i];
	}

	current_config->LEFT_MOUSE_KEY = mouse_btn_key_codes_[0];
	current_config->RIGHT_MOUSE_KEY = mouse_btn_key_codes_[1];
	current_config->MIDDLE_MOUSE_KEY = mouse_btn_key_codes_[2];

	current_config->TOGGLE_MODIFIER = scancodes_to_glfw_[glfw_modifiers[modifier_selected]];
	current_config->TOGGLE_KEY = scancodes_to_glfw_[glfw_keys[key_selected]];

	current_config->Save(INI_FILE);
}

void MainView::ReadConfig()
{
	auto new_conf = Config::LoadNew(INI_FILE);
	if (!new_conf)
		return;

	for (int i = 0; i < 4; i++)
	{
		r_btn_key_codes_[i] = new_conf->RIGHT_STICK_KEYS[i];
	}

	mouse_btn_key_codes_[0] = new_conf->LEFT_MOUSE_KEY;
	mouse_btn_key_codes_[1] = new_conf->RIGHT_MOUSE_KEY;
	mouse_btn_key_codes_[2] = new_conf->MIDDLE_MOUSE_KEY;

	Application::GetInstance()->Reconfig(new_conf);
}
