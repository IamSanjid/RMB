#pragma once

#include "../native.h"
#include <X11/Xlib.h>
#include <X11/X.h>
#include <unordered_map>
#include <vector>

class LinuxNative : public Native
{
public:
	LinuxNative();

	LinuxNative(const LinuxNative&) = delete;
	LinuxNative& operator=(const LinuxNative&) = delete;

	static LinuxNative* GetInstance();

	~LinuxNative() override;

	void RegisterHotKey(uint32_t key, uint32_t modifier) override;
	void UnregisterHotKey(uint32_t key, uint32_t modifier) override;
	void SendKeysDown(uint32_t* keys, size_t count) override;
	void SendKeysUp(uint32_t* keys, size_t count) override;
	void SetMousePos(double x, double y) override;
	bool SetFocusOnProcess(const std::string& process_name) override;
	void CursorHide(bool hide) override;
	void Update() override;
	void GetMousePos(double* x_ret, double* y_ret) override;

private:
	struct ScanCodeInfo
	{
		uint32_t group;
		uint32_t modmask;
		KeySym symbol;
	};

	struct RegKey
	{
		uint32_t key, modifier;
	};

    static LinuxNative* instance_;

	typedef void(*EnumWindowType)(Window window, void* userDefinedPtr);

	/* most of these codes are copy pasted from https://github.com/jordansissel/xdotool */

	bool GetDefaultScreenMousePos(int* x_ret, int* y_ret, int* screen_ret = NULL, Window* window_ret = NULL);
	unsigned char* GetWindowPropertyByAtom(Window window, Atom atom, long *nitems = NULL, Atom *type = NULL, int *size = NULL);
	uint32_t KeyCodeToModifier(XModifierKeymap *modmap, KeyCode keycode);
	uint32_t HashRegKey(int key, uint32_t modmask);
	void EnumAllWindow(EnumWindowType enumWindowProc, void* userDefinedPtr);

	bool ActivateWindow(Window window);
	void SendKey(int key, bool is_down);
	void SendModifier(int modmask, int is_press);

	std::unordered_map<uint32_t, ScanCodeInfo> scan_code_infos_;
	std::unordered_map<uint32_t, RegKey> registered_keys_;

	const char* display_name_ = NULL;
	Display* display_;
};