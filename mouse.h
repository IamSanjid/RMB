#pragma once
#include "vec.h"
#include <vector>

class Mouse
{
public:
	void MouseMoved(double x, double y, double center_x, double center_y);
	void TurnTest(int delay, int type);
};