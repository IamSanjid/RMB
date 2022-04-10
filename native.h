#pragma once
#include <cstdint>
#include <string>
#include <memory>

#include "EventSystem.h"

struct HotkeyEvent : Event
{
	HotkeyEvent(uint32_t key_, uint32_t modifier_)
		: key(key_), modifier(modifier_) {};
	uint32_t key;
	uint32_t modifier;
};

class Native
{
public:
	Native() {};
	Native(const Native&) = delete;
	Native& operator=(const Native&) = delete;

	static Native* GetInstance();

	virtual ~Native() = default;
	/* the keys are expected to be the scan code for the specific key on the specific platform */
	virtual void RegisterHotKey(uint32_t key, uint32_t modifier) = 0;
	virtual void UnregisterHotKey(uint32_t key, uint32_t modifier) = 0;
	virtual void SendKeysDown(uint32_t* keys, size_t count) = 0;
	virtual void SendKeysUp(uint32_t* keys, size_t count) = 0;
	virtual void SetMousePos(int x, int y) = 0;
	virtual void GetMousePos(int* x_ret, int* y_ret) = 0;
	/* optional */
	virtual bool SetFocusOnProcess(const std::string& process_name) = 0;
	virtual void CursorHide(bool hide) = 0;
	/* should not block the current thread */
	virtual void Update() = 0;
};