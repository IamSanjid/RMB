#pragma once
#include <thread>
#include "vec.h"

class Mouse {
public:
    Mouse();
    void MouseMoved(int x, int y, int center_x, int center_y);

#if _DEBUG
    void TurnTest(int delay, int type);
#endif

private:
    void UpdateThread(std::stop_token stop_token);
    void StopPanning();

    vf2d last_mouse_change_{};
    int mouse_panning_timeout_{};
    std::jthread update_thread;
};