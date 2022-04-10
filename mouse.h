#pragma once
#include "vec.h"

class Controller;

class Mouse
{
public:
	void MouseMoved(int x, int y, int center_x, int center_y);
	void Update(Controller* controller);
	void StopPanning();

#if _DEBUG
	void TurnTest(int delay, int type);
#endif

private:

	vf2d last_mouse_change_{};
	int mouse_panning_timeout_{};
};