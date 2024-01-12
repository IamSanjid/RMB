#include "keyboard_manager.h"

#ifndef _WIN32
#include <string.h>
#endif

std::shared_ptr<KeyboardManager> KeyboardManager::GetInstance() {
    static std::shared_ptr<KeyboardManager> singleton_(new KeyboardManager());
    return singleton_;
}

KeyboardManager::KeyboardManager() {
    update_thread_ = std::jthread([this](std::stop_token stop_token) { UpdateThread(stop_token); });
}

KeyboardManager::~KeyboardManager() {
    update_thread_.request_stop();
    ClearDownKeys();
}

void KeyboardManager::SetPersistentMode(bool value) {
    persistent_mode_.store(value, std::memory_order_release);
}

void KeyboardManager::Clear() {
    clear_requested_.store(true, std::memory_order_release);
}

void KeyboardManager::UpdateThread(std::stop_token stop_token) {
    constexpr int max_dequeue_items = 4 * 2;
    KeysBitset keys[max_dequeue_items]{};

    while (!stop_token.stop_requested()) {
        bool updated_down_keys = false;
        size_t keys_cnt = 0;

        if (clear_requested_.load(std::memory_order_acquire)) {
            ClearDownKeys();
            memset(keys, 0, sizeof(KeysBitset) * max_dequeue_items);
            continue;
        }

        do {
            keys_cnt = down_keys_queue_.try_dequeue_bulk(keys, max_dequeue_items);
            if (keys_cnt) {
                for (size_t i = 0; i < keys_cnt; i++) {
                    down_keys_in_ |= keys[i];
                }
                updated_down_keys = true;
                memset(keys, 0, sizeof(KeysBitset) * max_dequeue_items);
            }
        } while (keys_cnt != 0);

        do {
            keys_cnt = up_keys_queue_.try_dequeue_bulk(keys, max_dequeue_items);
            if (keys_cnt) {
                for (size_t i = 0; i < keys_cnt; i++) {
                    Native::GetInstance()->SendKeysBitsetUp(keys[i]);
                    down_keys_in_ &= ~(keys[i]);
                }
                memset(keys, 0, sizeof(KeysBitset) * max_dequeue_items);
            }
        } while (keys_cnt != 0);

        if (updated_down_keys || persistent_mode_.load(std::memory_order_acquire)) {
            Native::GetInstance()->SendKeysBitsetDown(down_keys_in_);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ClearDownKeys();
    fprintf(stdout, "Exiting keyboard manager...");
}

void KeyboardManager::ClearDownKeys() {
    constexpr int max_dequeue_items = 4 * 2;
    KeysBitset keys[max_dequeue_items]{};
    size_t keys_cnt = 0;

    // up all the down keys...
    Native::GetInstance()->SendKeysBitsetUp(down_keys_in_);
    down_keys_in_.reset();
    do {
        keys_cnt = down_keys_queue_.try_dequeue_bulk(keys, max_dequeue_items);
    } while (keys_cnt != 0);

    do {
        keys_cnt = up_keys_queue_.try_dequeue_bulk(keys, max_dequeue_items);
    } while (keys_cnt != 0);
    clear_requested_.store(false, std::memory_order_release);
}
