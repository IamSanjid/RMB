#include <cmath>
#include <deque>
#include <stdint.h>
#include "npad_controller.h"

#include "Application.h"
#include "Config.h"
#include "Utils.h"
#include "concurrentqueue.h"
#include "native.h"

constexpr int BUTTONS = 4;

struct StickStatus {
    int16_t x;
    int16_t y;
};

struct ButtonStatus {
    bool reset;
    int button;
};

class StickInputHandler {
public:
    void OnUpdate() {
        auto config = Config::Current();

        constexpr int max_timeouts_to_dequeue = BUTTONS * 2;
        size_t timeouts_cnt = 0;
        ButtonTimeout timeouts[max_timeouts_to_dequeue];

        if (stopped_) {
            for (int i = 0; i < BUTTONS; i++) {
                if (timeouts_[i] != 0) {
                    Native::GetInstance()->SendKeysUp(&config->RIGHT_STICK_KEYS[i], 1);
                }
                timeouts_[i] = 0;
            }

            do {
                timeouts_cnt = timeout_queue_.try_dequeue_bulk(timeouts, max_timeouts_to_dequeue);
            } while (timeouts_cnt != 0);
            stopped_ = false;
            return;
        }

        do {
            timeouts_cnt = timeout_queue_.try_dequeue_bulk(timeouts, max_timeouts_to_dequeue);
            for (size_t i = 0; i < timeouts_cnt; i++) {
                auto& timeout = timeouts[i];
                if (timeout.time == 0.0) {
                    Native::GetInstance()->SendKeysUp(&config->RIGHT_STICK_KEYS[timeout.button], 1);
                    timeouts_[timeout.button] = 0;
                }
                else if (timeouts_[timeout.button] == 0) {
                    Native::GetInstance()->SendKeysDown(&config->RIGHT_STICK_KEYS[timeout.button],
                                                        1);
                    timeouts_[timeout.button] = timeout.time;
                }
#ifdef _DEBUG
                printf("button: %d, time: %d\n", timeout.button, timeout.time);
#endif
            }
        } while (timeouts_cnt != 0);
    }

    inline void OnChange(const StickStatus& status) {
        if (stopped_)
            return;

        auto value_x = status.x;
        auto value_y = status.y;

#ifdef _DEBUG
        printf("stick change: %d, %d\n", value_x, value_y);
#endif

        auto new_time_x = static_cast<uint16_t>(std::abs(value_x));
        auto new_time_y = static_cast<uint16_t>(std::abs(value_y));

        auto button_x = (0 * 2) + (Utils::sign(value_x) > 0);
        auto opposite_button_x = 1 - button_x;

        auto button_y = (1 * 2) + (Utils::sign(value_y) > 0);
        auto opposite_button_y = 5 - button_y;

        ButtonTimeout timeouts[BUTTONS]{{button_y, new_time_y},
                                        {opposite_button_y, 0},
                                        {button_x, new_time_x},
                                        {opposite_button_x, 0}};

        bool prioritize_y = new_time_y > new_time_x;
        timeouts[(prioritize_y * 2)] = {button_x, new_time_x};
        timeouts[(prioritize_y * 2) + 1] = {opposite_button_x, 0};
        timeouts[(prioritize_y ^ 1) * 2] = {button_y, new_time_y};
        timeouts[((prioritize_y ^ 1) * 2) + 1] = {opposite_button_y, 0};

        timeout_queue_.enqueue_bulk(timeouts, BUTTONS);
        /*for (int index = 0; index < 2; index++)
        {
            float value = status.x;
            int not_reset = ((status.value & (index + 1)) >> index) ^ 1;
            float time = (float)not_reset;

            if (index == 1)
            {
                value = status.y;
            }

            time *= std::abs(value);

            int button = (index * 2) + (Utils::sign(value) == 1);
            int opposite_button = (index * 2) + (Utils::sign(value) != 1);

            // unnecessary stuff just wanted to test pre-calculated label addresses

#if defined(__clang__) || defined(__GNUC__)
            constexpr void* keys_gotos[] = {&&UP, &&DOWN};

            goto *keys_gotos[not_reset];
        UP:
            Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[button], 1);
            goto ADD;
        DOWN:
            Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[button], 1);
            goto ADD;


        ADD:
#else
            if (!not_reset)
            {
                Native::GetInstance()->SendKeysUp(&Config::Current()->RIGHT_STICK_KEYS[button], 1);
            }
            else
            {
                Native::GetInstance()->SendKeysDown(&Config::Current()->RIGHT_STICK_KEYS[button],
1);
            }
#endif
            ButtonTimeout timeouts[2] =
            {
                { button, time },
                { opposite_button, 0.f }
            };
            timeout_queue_.enqueue_bulk(timeouts, 2);
        }*/
    }

    void OnStop() {
        stopped_ = true;
    }

private:
    struct ButtonTimeout {
        int button;
        uint16_t time;
    };

    uint16_t timeouts_[BUTTONS]{};
    bool stopped_{};
    moodycamel::ConcurrentQueue<ButtonTimeout> timeout_queue_{};
};

class ButtonInputHandler {
public:
    void OnUpdate() {
        if (stopped_) {
            std::scoped_lock<std::mutex> lock{mutex};
            for (auto& it : pressed_buttons_) {
                if (it.second) {
                    Native::GetInstance()->SendKeysUp((uint32_t*)&it.first, 1);
                }
            }
            pressed_buttons_.clear();
            stopped_ = false;
        }
    }

    void OnChange(const ButtonStatus& status) {
        if (stopped_)
            return;

        if (status.reset) {
            Native::GetInstance()->SendKeysUp((uint32_t*)&status.button, 1);
        }
        else {
            Native::GetInstance()->SendKeysDown((uint32_t*)&status.button, 1);
        }
        std::scoped_lock<std::mutex> lock{mutex};
        pressed_buttons_[status.button] = !status.reset;
    }

    void OnStop() {
        stopped_ = true;
    }

private:
    bool stopped_{};
    std::unordered_map<uint32_t, bool> pressed_buttons_;
    mutable std::mutex mutex;
};

NpadController::NpadController()
    : stick_handler_(new StickInputHandler()), button_input_handler_(new ButtonInputHandler()) {
    update_thread = std::jthread([this](std::stop_token stop_token) { UpdateThread(stop_token); });
}

void NpadController::UpdateThread(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        stick_handler_->OnUpdate();
        button_input_handler_->OnUpdate();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ClearState();
    delete stick_handler_;
    delete button_input_handler_;

    fprintf(stdout, "Exiting Controller.\n");
}

void NpadController::SetStick(float raw_x, float raw_y) {
    std::scoped_lock<std::mutex> lock{mutex};
    if (last_raw_x_ == raw_x && last_raw_y_ == raw_y) {
        return;
    }

#if _DEBUG
    fprintf(stdout, "new change: %f, %f\n", raw_x, raw_y);
#endif
    auto current_config = Config::Current();
    last_raw_x_ = raw_x;
    last_raw_y_ = raw_y;

    SanatizeAxes(last_raw_x_, last_raw_y_, true);

    // auto last_x = axes_.x, last_y = axes_.y;

    auto new_x = std::roundf(last_x_ * 272.0f) /** camera_update*/;
    auto new_y = std::roundf(last_y_ * 272.0f) /** camera_update*/;

    /*if (current_config->SPECIAL_ROUNDING) {
        const float x_abs = std::abs(new_x);
        const float y_abs = std::abs(new_y);
        const float sum = x_abs + y_abs;

        if (sum > 0.f) {
            const float difference_x = x_abs / sum;
            const float difference_y = y_abs / sum;

            if (std::abs(difference_x - difference_y) >= current_config->DEADZONE) {
                if (difference_x > difference_y) {
                    new_y = std::round(new_y);
                }
                else {
                    new_x = std::round(new_x);
                }
            }
        }
    }
    else {
        new_x = std::round(new_x);
        new_y = std::round(new_y);
    }*/

    // bool reset_x = static_cast<int>(new_x) == 0;
    // bool reset_y = static_cast<int>(new_y) == 0;

    // new_x += reset_x * last_x;
    // new_y += reset_y * last_y;

    stick_handler_->OnChange({static_cast<int16_t>(new_x), static_cast<int16_t>(new_y)});

    axes_.x = new_x;
    axes_.y = new_y;

    axes_.right = last_x_ > current_config->THRESHOLD;
    axes_.left = last_x_ < -current_config->THRESHOLD;
    axes_.up = last_y_ > current_config->THRESHOLD;
    axes_.down = last_y_ < -current_config->THRESHOLD;

#if _DEBUG
    if (axes_.x != 0 || axes_.y != 0) {
        fprintf(stdout, "axes_change: %f, %f - actual_value: %f, %f\n", axes_.x, axes_.y, last_x_,
                last_y_);
        fprintf(stdout, "right: %d left: %d up: %d down: %d\n", axes_.right, axes_.left, axes_.up,
                axes_.down);
    }
#endif
}

void NpadController::SetButton(int button, int value) {
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

    if (!std::isnormal(raw_x))
        raw_x = 0;
    if (!std::isnormal(raw_y))
        raw_y = 0;

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
