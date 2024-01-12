#pragma once

#include "native.h"
#include "concurrentqueue.h"

class KeyboardManager {
public:
    static std::shared_ptr<KeyboardManager> GetInstance();

    KeyboardManager();
    ~KeyboardManager();

    inline void SendKeysDown(KeysBitset keys) {
        down_keys_queue_.enqueue(keys);
    }

    inline void SendKeysUp(KeysBitset keys) {
        up_keys_queue_.enqueue(keys);
    }

    inline void SendKeyDown(uint32_t key) {
        KeysBitset current_set{};
        current_set.set(key);
        down_keys_queue_.enqueue(current_set);
    }

    inline void SendKeyUp(uint32_t key) {
        KeysBitset current_set{};
        current_set.set(key);
        up_keys_queue_.enqueue(current_set);
    }
    
    void SetPersistentMode(bool value = true);

    void Clear();

    bool IsPersistent() const {
        return persistent_mode_.load(std::memory_order_acquire);
    }

private:
    void UpdateThread(std::stop_token stop_token);

    void ClearDownKeys();

    moodycamel::ConcurrentQueue<KeysBitset> down_keys_queue_;
    moodycamel::ConcurrentQueue<KeysBitset> up_keys_queue_;

    KeysBitset down_keys_in_;
    std::jthread update_thread_;
    std::atomic_bool persistent_mode_ = false;
    std::atomic_bool clear_requested_ = false;
};