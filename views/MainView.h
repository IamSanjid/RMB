#pragma once

#include "View.h"
#include <vector>
#include <string>

class MainView : public View
{
public:
	MainView();
	~MainView();

	void Show() override;
	void OnKeyPress(int key, int scancode, int mods) override;
	void SetSize(uint32_t width, uint32_t height) override { (void)width; (void)height; }
	void SetPos(uint32_t x, uint32_t y) override { (void)x; (void)y; }

#if _DEBUG
	int test_delay = 10;
	int test_type = 0;
#endif

private:
	void GetRightStickButtons();

	int r_btn_key_codes_[4]{};
	std::string r_btn_text_[4];
	bool r_btn_changing_[4]{ false };

	int modifier_selected = 0, key_selected = 8;
	bool show_conf_input = false;

	const char* modifiers[3] = { "Control", "Shift", "Alt" };
	const uint32_t* glfw_modifiers;
	std::vector<uint32_t> glfw_keys;
	std::vector<const char*> glfw_str_keys;
};