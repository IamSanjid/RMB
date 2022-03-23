#pragma once
#include "vec.h"
class Mouse
{
public:
	void MouseMoved(double x, double y, double center_x, double center_y);
	void TurnTest(int delay, int type);
	void Update();
	void StopPanning();

private:
	void SetCamera(const double& sensitivity);

	vd2d last_axis_change_{};
	double axis_change_timeouts_[4]{};
	double release_timeouts_[4]{};

	int mouse_panning_timeout_;
};