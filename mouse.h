#pragma once
#include "vec.h"

class Controller;

class Mouse
{
public:
	void MouseMoved(int x, int y, int center_x, int center_y);
	void TurnTest(int delay, int type);
	void Update(Controller* controller);
	void StopPanning();

private:

	vf2d last_mouse_change_{};

	int mouse_panning_timeout_{};
};