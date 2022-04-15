#pragma once

#include <cstdint>

class View
{
public:
	virtual ~View() = default;

	virtual void SetSize(uint32_t width, uint32_t height) = 0;
	virtual void SetPos(uint32_t x, uint32_t y) = 0;
	virtual void Show() = 0;
	virtual void OnKeyPress(int key, int scancode, int mods) = 0;

protected:
	const char* name_ = "View";
};