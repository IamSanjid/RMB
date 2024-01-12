#include <cmath>
#include <deque>
#include <stdint.h>
#include "npad_controller.h"

#include <bitset>
#include "Application.h"
#include "Config.h"
#include "Utils.h"
#include "keyboard_manager.h"

constexpr int BUTTONS = 4;

constexpr int32_t HID_JOYSTICK_MAX = 0x7fff;

struct StickStatus {
    int32_t x;
    int32_t y;
};

struct ButtonStatus {
    bool reset;
    uint32_t button;
};

class StickInputHandler {
public:
    inline void OnChange(const StickStatus& status) {
        auto config = Config::Current();

        auto value_x = status.x;
        auto value_y = status.y;

        DEBUG_OUT("stick change: %d, %d\n", value_x, value_y);

        auto new_time_x = static_cast<uint32_t>(std::abs(value_x));
        auto new_time_y = static_cast<uint32_t>(std::abs(value_y));

        auto button_x = (0 * 2) + (Utils::sign(value_x) > 0);
        auto opposite_button_x = 1 - button_x;

        auto button_y = (1 * 2) + (Utils::sign(value_y) > 0);
        auto opposite_button_y = 5 - button_y;

        if (new_time_x == 0) {
            KeyboardManager::GetInstance()->SendKeyUp(config->RIGHT_STICK_KEYS[button_x]);
        }
        else if (timeouts_[button_x] == 0) {
            KeyboardManager::GetInstance()->SendKeyDown(config->RIGHT_STICK_KEYS[button_x]);
        }

        if (timeouts_[opposite_button_x])
            KeyboardManager::GetInstance()->SendKeyUp(config->RIGHT_STICK_KEYS[opposite_button_x]);

        if (new_time_y == 0) {
            KeyboardManager::GetInstance()->SendKeyUp(config->RIGHT_STICK_KEYS[button_y]);
        }
        else if (timeouts_[button_y] == 0) {
            KeyboardManager::GetInstance()->SendKeyDown(config->RIGHT_STICK_KEYS[button_y]);
        }

        if (timeouts_[opposite_button_y])
            KeyboardManager::GetInstance()->SendKeyUp(config->RIGHT_STICK_KEYS[opposite_button_y]);

        timeouts_[opposite_button_x] = 0;
        timeouts_[opposite_button_y] = 0;
        timeouts_[button_x] = new_time_x;
        timeouts_[button_y] = new_time_y;
    }

    inline void Clear() {
        memset(timeouts_, 0, sizeof(uint32_t) * BUTTONS);
    }
    
private:
    uint32_t timeouts_[BUTTONS]{0};
};

NpadController::NpadController()
    : stick_handler_(new StickInputHandler()) {
}

NpadController::~NpadController() {
    ClearState();
    delete stick_handler_;

    fprintf(stdout, "Exiting Controller.\n");
}

/* There should be one thread calling this function at a time... */
void NpadController::SetStick(float raw_x, float raw_y) {
    std::scoped_lock<std::mutex> lock{mutex};
    if (last_raw_x_ == raw_x && last_raw_y_ == raw_y) {
        return;
    }

    DEBUG_OUT("new change: %f, %f\n", raw_x, raw_y);

    auto current_config = Config::Current();
    last_raw_x_ = raw_x;
    last_raw_y_ = raw_y;

    SanatizeAxes(last_raw_x_, last_raw_y_, true);

    auto new_x = std::roundf(last_x_ * static_cast<float>(HID_JOYSTICK_MAX));
    auto new_y = std::roundf(last_y_ * static_cast<float>(HID_JOYSTICK_MAX));

    stick_handler_->OnChange({static_cast<int32_t>(new_x), static_cast<int32_t>(new_y)});

    axes_.x = new_x;
    axes_.y = new_y;

    axes_.right = last_x_ > current_config->THRESHOLD;
    axes_.left = last_x_ < -current_config->THRESHOLD;
    axes_.up = last_y_ > current_config->THRESHOLD;
    axes_.down = last_y_ < -current_config->THRESHOLD;

    DEBUG_OUT_COND(axes_.x != 0 || axes_.y != 0, "axes_change: %f, %f - actual_value: %f, %f\n",
                   axes_.x, axes_.y, last_x_, last_y_);
    DEBUG_OUT_COND(axes_.x != 0 || axes_.y != 0, "right: %d left: %d up: %d down: %d\n",
                   axes_.right, axes_.left, axes_.up, axes_.down);
}

void NpadController::SetButton(uint32_t button, int value) {
    if (value) {
        KeyboardManager::GetInstance()->SendKeyDown(button);
    }
    else {
        KeyboardManager::GetInstance()->SendKeyUp(button);
    }
}

void NpadController::SetPersistentMode(bool value) {
    KeyboardManager::GetInstance()->SetPersistentMode(value);
}

void NpadController::ClearState() {
    last_raw_x_ = last_raw_y_ = last_x_ = last_y_ = 0.f;
    axes_ = {};

    stick_handler_->Clear();
    KeyboardManager::GetInstance()->Clear();
}

void NpadController::SanatizeAxes(float raw_x, float raw_y, bool clamp_value) {
    auto current_config = Config::Current();
    float& x = last_x_;
    float& y = last_y_;

    if (!std::isnormal(raw_x)) {
        raw_x = 0;
    }
    if (!std::isnormal(raw_y)) {
        raw_y = 0;
    }

    raw_x += current_config->X_OFFSET;
    raw_y += current_config->Y_OFFSET;

    if (std::abs(current_config->X_OFFSET) < 0.75f) {
        if (raw_x > 0) {
            raw_x /= 1 + current_config->X_OFFSET;
        }
        else {
            raw_x /= 1 - current_config->X_OFFSET;
        }
    }

    if (std::abs(current_config->Y_OFFSET) < 0.75f) {
        if (raw_y > 0) {
            raw_y /= 1 + current_config->Y_OFFSET;
        }
        else {
            raw_y /= 1 - current_config->Y_OFFSET;
        }
    }

    x = raw_x;
    y = raw_y;

    float r = x * x + y * y;
    r = std::sqrt(r);

    if (r <= current_config->DEADZONE || current_config->DEADZONE >= 1.0f) {
        x = 0;
        y = 0;
        return;
    }

    const float deadzone_factor =
        1.0f / r * (r - current_config->DEADZONE) / (1.0f - current_config->DEADZONE);
    x = x * deadzone_factor / current_config->RANGE;
    y = y * deadzone_factor / current_config->RANGE;
    r = r * deadzone_factor / current_config->RANGE;

    if (clamp_value && r > 1.0f) {
        x /= r;
        y /= r;
    }
}