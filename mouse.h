#pragma once
#include "vec.h"
#include <vector>

class Mouse
{
public:
	void MouseMoved(double x, double y, double center_x, double center_y);
	void TurnTest(int delay, int type);
	void Update();

private:
	std::vector<uint32_t> keys_;
	double key_up_time_ = 0.0;
};