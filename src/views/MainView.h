#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "View.h"

/* this is a mess don't care bout it much */

class MainView : public View {
public:
    MainView();
    ~MainView();

    void Show() override;
    void OnKeyRelease(int key, int scancode, int mods) override;
    void SetSize(uint32_t width, uint32_t height) override {
        (void)width;
        (void)height;
    }
    void SetPos(uint32_t x, uint32_t y) override {
        (void)x;
        (void)y;
    }

#if _DEBUG
    int test_delay = 10;
    int test_type = 0;
#endif

private:
    void GetToggleKeys();
    void GetRightStickButtons();
    void GetMouseButtons();
    void SaveConfig();
    void ReadConfig();

    std::unordered_map<int, bool> selected_keys_;

    bool any_mouse_btn_changing_ = false;
    bool any_r_btn_chaniging_ = false;

    int r_btn_key_codes_[4]{};
    std::string r_btn_text_[4];
    bool r_btn_changing_[4]{false};

    int mouse_btn_key_codes_[3]{};
    std::string mouse_btn_text_[3];
    bool mouse_btn_changing_[3]{false};

    int toggle_modifier_selected = -1, toggle_key_selected = -1;
};