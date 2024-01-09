#include <cmath>
#include <deque>
#include <stdint.h>
#include "npad_controller.h"

#include <bitset>
#include "Application.h"
#include "Config.h"
#include "Utils.h"
#include "concurrentqueue.h"
#include "native.h"

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
    StickInputHandler() : prod_token_(timeout_queue_){};

    void OnUpdate() {
        auto config = Config::Current();

        constexpr int max_timeouts_to_dequeue = BUTTONS * 2;
        size_t timeouts_cnt = 0;
        ButtonTimeout timeouts[max_timeouts_to_dequeue];

        do {
            timeouts_cnt = timeout_queue_.try_dequeue_bulk_from_producer(prod_token_, timeouts,
                                                                         max_timeouts_to_dequeue);
            for (size_t i = 0; i < timeouts_cnt; i++) {
                auto& timeout = timeouts[i];
                if (timeout.button == -1) [[unlikely]] {
                    clear_ = true;
                    Clear();
                    return;
                }

                if (timeout.time == 0) {
                    Native::GetInstance()->SendKeysUp(&config->RIGHT_STICK_KEYS[timeout.button], 1);
                    timeouts_[timeout.button] = 0;
                }
                else if (timeouts_[timeout.button] == 0) {
                    Native::GetInstance()->SendKeysDown(&config->RIGHT_STICK_KEYS[timeout.button],
                                                        1);
                    timeouts_[timeout.button] = timeout.time;
                }

                DEBUG_OUT("button: %d, time: %d\n", timeout.button, timeout.time);
            }
        } while (timeouts_cnt != 0);
    }

    inline void OnChange(const StickStatus& status) {
        if (clear_) [[unlikely]] {
            return;
        }

        auto value_x = status.x;
        auto value_y = status.y;

        DEBUG_OUT("stick change: %d, %d\n", value_x, value_y);

        auto new_time_x = static_cast<uint32_t>(std::abs(value_x));
        auto new_time_y = static_cast<uint32_t>(std::abs(value_y));

        auto button_x = (0 * 2) + (Utils::sign(value_x) > 0);
        auto opposite_button_x = 1 - button_x;

        auto button_y = (1 * 2) + (Utils::sign(value_y) > 0);
        auto opposite_button_y = 5 - button_y;

        ButtonTimeout timeouts[BUTTONS]{};

        bool prioritize_y = new_time_y > new_time_x && new_time_x > 0;
        timeouts[(prioritize_y * 2)] = {button_x, new_time_x};
        timeouts[(prioritize_y * 2) + 1] = {opposite_button_x, 0};
        timeouts[(prioritize_y ^ 1) * 2] = {button_y, new_time_y};
        timeouts[((prioritize_y ^ 1) * 2) + 1] = {opposite_button_y, 0};

        timeout_queue_.enqueue_bulk(prod_token_, timeouts, BUTTONS);
    }

    void OnStop() {
        timeout_queue_.enqueue({-1, 0});
    }

private:
    inline void Clear() {
        auto config = Config::Current();

        constexpr int max_timeouts_to_dequeue = BUTTONS * 2;
        size_t timeouts_cnt = 0;
        ButtonTimeout timeouts[max_timeouts_to_dequeue];

        for (int i = 0; i < BUTTONS; i++) {
            if (timeouts_[i] != 0) {
                Native::GetInstance()->SendKeysUp(&config->RIGHT_STICK_KEYS[i], 1);
            }
            timeouts_[i] = 0;
        }

        do {
            timeouts_cnt = timeout_queue_.try_dequeue_bulk(timeouts, max_timeouts_to_dequeue);
        } while (timeouts_cnt != 0);

        clear_ = false;
    }

private:
    struct ButtonTimeout {
        int button;
        uint32_t time;
    };

    bool clear_ = false;
    uint32_t timeouts_[BUTTONS]{};

    moodycamel::ConcurrentQueue<ButtonTimeout> timeout_queue_{};
    /* single-producer-single-consumer case, so it's safe. */
    moodycamel::ProducerToken prod_token_;
};

class ButtonInputHandler {
public:
    inline void OnChange(const ButtonStatus& status) {
        if (status.reset) {
            Native::GetInstance()->SendKeysUp((uint32_t*)&status.button, 1);
        }
        else {
            Native::GetInstance()->SendKeysDown((uint32_t*)&status.button, 1);
        }
        std::scoped_lock<std::mutex> lock{mutex};
        pressed_buttons_[status.button] = !status.reset;
    }

    inline void OnStop() {
        std::scoped_lock<std::mutex> lock{mutex};
        for (uint32_t i = 0; i < 256; i++) {
            if (pressed_buttons_[i]) {
                Native::GetInstance()->SendKeysUp(&i, 1);
            }
        }
        pressed_buttons_.reset();
    }

private:
    std::bitset<256> pressed_buttons_{};
    mutable std::mutex mutex{};
};

NpadController::NpadController()
    : stick_handler_(new StickInputHandler()), button_input_handler_(new ButtonInputHandler()) {
    update_thread = std::jthread([this](std::stop_token stop_token) { UpdateThread(stop_token); });
}

void NpadController::UpdateThread(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        stick_handler_->OnUpdate();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ClearState();
    delete stick_handler_;
    delete button_input_handler_;

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

    auto new_x = std::roundf(last_x_ * HID_JOYSTICK_MAX);
    auto new_y = std::roundf(last_y_ * HID_JOYSTICK_MAX);

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
    button_input_handler_->OnChange({value == 0, button});
}

void NpadController::ClearState() {
    last_raw_x_ = last_raw_y_ = last_x_ = last_y_ = 0.f;
    axes_ = {};

    stick_handler_->OnStop();
    button_input_handler_->OnStop();
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