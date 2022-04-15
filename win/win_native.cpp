#include "win_native.h"
#include "Utils.h"

#include <chrono>
#include <thread>

std::shared_ptr<Native> Native::GetInstance()
{
	static std::shared_ptr<Native> singleton_(WinNative::GetInstance());
	return singleton_;
}

WinNative* WinNative::GetInstance()
{
	if (!instance_)
		instance_ = new WinNative();
	return instance_;
}

WinNative* WinNative::instance_ = nullptr;

WinNative::WinNative()
	: /*kbd_hook_(SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, NULL)),
	last_key_({})*/
	mouse_hook_(SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, NULL, NULL))
{
	HANDLE arrowHandle = LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
	default_arrow_ = CopyCursor(arrowHandle);

	WNDCLASSEXW wcex{};

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hIcon = 0;
	wcex.hCursor = 0;
	wcex.hbrBackground = 0;
	wcex.lpszMenuName = 0;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = 0;

	RegisterClassEx(&wcex);

	reg_window_ = CreateWindow(szWindowClass, L"win native reg window", 0,
		CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, nullptr, nullptr, 0, nullptr);
}

WinNative::~WinNative()
{
	/*if (kbd_hook_)
		UnhookWindowsHookEx(kbd_hook_);*/
	if (mouse_hook_)
	{
		UnhookWindowsHookEx(mouse_hook_);
	}
	if (reg_window_)
	{
		DestroyWindow(reg_window_);
		UnregisterClass(szWindowClass, nullptr);
	}
	if (default_arrow_)
	{
		SetSystemCursor(default_arrow_, OCR_NORMAL);
		DestroyCursor(default_arrow_);
	}
	registered_keys_.clear();
	instance_ = nullptr;
}

void WinNative::RegisterHotKey(uint32_t key, uint32_t modifier)
{
	std::string key_combo = std::to_string(modifier) + "+" + std::to_string(key);
	if (registered_keys_.find(key_combo) == registered_keys_.cend())
	{
		auto id = GetAvailableId();
		if (id == 0)
		{
			printf("Failed to register hotkey, no id available...\n");
			return;
		}
		registered_keys_[key_combo] = { id, key, modifier };
		registered_ids_[id] = key_combo;
		modifier = modifier != 0 ? GetModKeyFrom(modifier) : MOD_NOREPEAT;
		key = MapVirtualKey(key, 1);
		if (!key || !modifier)
		{
			printf("Failed to register hotkey, couldn't map the given GLFW key with the native platform virtual key...\n");
			return;
		}
		if (::RegisterHotKey(reg_window_, id, modifier, key))
			printf("Registered: %s\n", key_combo.c_str());
	}
}

void WinNative::UnregisterHotKey(uint32_t key, uint32_t modifier)
{
	std::string key_combo = std::to_string(modifier) + "+" + std::to_string(key);
	auto found = registered_keys_.find(key_combo);
	if (found != registered_keys_.cend())
	{
		printf("Unregistered: %s\n", key_combo.c_str());
		::UnregisterHotKey(reg_window_, found->second.id);
		registered_ids_.erase(found->second.id);
		registered_keys_.erase(key_combo);
	}
}

void WinNative::SendKeysDown(uint32_t* keys, size_t count)
{
	std::vector<INPUT> input(count);
	ZeroMemory(&input[0], input.size() * sizeof(INPUT));
	for (auto i = 0u; i < count; i++)
	{
		auto ch = (WORD)keys[i];
		if (!ch)
			return;
		input[i].type = INPUT_KEYBOARD;
		input[i].ki.dwFlags = KEYEVENTF_SCANCODE;
		input[i].ki.wScan = ch;
	}
	SendInput((UINT)count, &input[0], (UINT)sizeof(INPUT));
}

void WinNative::SendKeysUp(uint32_t* keys, size_t count)
{
	std::vector<INPUT> input(count);
	ZeroMemory(&input[0], input.size() * sizeof(INPUT));
	for (auto i = 0u; i < count; i++)
	{
		auto ch = (WORD)keys[i];
		if (!ch)
			return;
		input[i].type = INPUT_KEYBOARD;
		input[i].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
		input[i].ki.wScan = ch;
	}
	SendInput((UINT)count, &input[0], (UINT)sizeof(INPUT));
}

void WinNative::SetMousePos(int x, int y)
{
	::SetCursorPos(x, y);
}

void WinNative::GetMousePos(int* x_ret, int* y_ret)
{
	POINT pos{};
	if (GetCursorPos(&pos))
	{
		*x_ret = static_cast<int>(pos.x);
		*y_ret = static_cast<int>(pos.y);
	}
	else
	{
		*x_ret = 0;
		*y_ret = 0;
	}
}

bool WinNative::IsMainWindowActive(const std::string& window_name)
{
	HWND active_window = ::GetForegroundWindow();
	if (!active_window)
		return false;
	DWORD id;
	GetWindowThreadProcessId(active_window, &id);

	char path[MAX_PATH];
	DWORD size = MAX_PATH;
	HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, id);
	QueryFullProcessImageNameA(hProc, 0, path, &size);
	CloseHandle(hProc);

	if (size && strstr(path, window_name.c_str()) != NULL
		&& GetWindow(active_window, GW_OWNER) == (HWND)0 && IsWindowVisible(active_window))
	{
		return true;
	}
	return false;
}

bool WinNative::SetFocusOnWindow(const std::string& window_name)
{
	struct process_finder
	{
		const char* sub_str;
		HWND found_process;
	};
	auto EnumWindowsProc = [](HWND hwnd, LPARAM lParam)
	{
		auto finder = (process_finder*)lParam;

		DWORD id;
		GetWindowThreadProcessId(hwnd, &id);

		char path[MAX_PATH];
		DWORD size = MAX_PATH;
		HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, id);
		QueryFullProcessImageNameA(hProc, 0, path, &size);
		CloseHandle(hProc);

		if (size && strstr(path, finder->sub_str) != NULL
			&& GetWindow(hwnd, GW_OWNER) == (HWND)0 && IsWindowVisible(hwnd))
		{
			finder->found_process = hwnd;
			return FALSE;
		}
		return TRUE;
	};
	process_finder finder = { window_name.c_str(), NULL };
	EnumWindows(EnumWindowsProc, (LPARAM)&finder);
	if (finder.found_process)
	{
		INPUT keyInput{};
		keyInput.type = INPUT_KEYBOARD;
		keyInput.ki.wVk = VK_MENU;
		keyInput.ki.dwFlags = 0;
		keyInput.ki.wScan = (WORD)MapVirtualKey(keyInput.ki.wVk, 0);
		SendInput(1, &keyInput, sizeof(INPUT));

		SetForegroundWindow(finder.found_process);
		SetFocus(finder.found_process);

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		keyInput.ki.wVk = VK_MENU;
		keyInput.ki.dwFlags = KEYEVENTF_KEYUP;
		SendInput(1, &keyInput, sizeof(INPUT));
		return true;
	}
	return false;
}

void WinNative::CursorHide(bool hide)
{
	static bool is_hidden = false;
	if (hide && !is_hidden)
	{
		// Save a copy of the default cursor
		if (!default_arrow_)
		{
			HANDLE arrowHandle = LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
			default_arrow_ = CopyCursor(arrowHandle);
		}

		// Set the cursor to a transparent one to emulate no cursor
		BYTE ANDmaskCursor[1 * 4]{ 0 };
		BYTE XORmaskCursor[1 * 4]{ 0 };

		auto noCursor = CreateCursor(nullptr, 1, 1, 1, 1, ANDmaskCursor, XORmaskCursor);
		SetSystemCursor(noCursor, OCR_NORMAL);
		DestroyCursor(noCursor);
		is_hidden = true;
	}
	else if (!hide && is_hidden)
	{
		SetSystemCursor(default_arrow_, OCR_NORMAL);
		DestroyCursor(default_arrow_);

		HANDLE arrowHandle = LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, 0, 0, LR_SHARED);
		default_arrow_ = CopyCursor(arrowHandle);
		is_hidden = false;
	}
}

uint32_t WinNative::GetModKeyFrom(uint32_t g_key)
{
	static std::unordered_map<uint32_t, uint32_t> m_key_map =
	{
		{ MapVirtualKey(VK_LCONTROL    , 0)             , MOD_CONTROL },
		{ MapVirtualKey(VK_RCONTROL    , 0)             , MOD_CONTROL },
		{ MapVirtualKey(VK_LMENU       , 0)             , MOD_ALT     },
		{ MapVirtualKey(VK_RMENU       , 0)             , MOD_ALT     },
		{ MapVirtualKey(VK_LSHIFT      , 0)             , MOD_SHIFT   },
		{ MapVirtualKey(VK_RSHIFT      , 0)             , MOD_SHIFT   }
	};
	if (m_key_map[g_key])
		return m_key_map[g_key];
	return 0;
}

uint32_t WinNative::GetAvailableId()
{
	// Valid hot key IDs are in the range 0x0000 to 0xBFFF. See
	// https://docs.microsoft.com/en-us/windows/desktop/api/winuser/nf-winuser-registerhotkey
	for (int i = 0x0001; i < 0xBFFF; i++) {
		if (registered_ids_.find(i) == registered_ids_.cend())
			return i;
	}
	return 0;
}

void WinNative::OnHotkey(uint32_t id)
{
	auto found = registered_ids_.find(id);
	if (found != registered_ids_.end())
	{
		auto& reg_key = registered_keys_[found->second];
		EventBus::Instance().publish(new HotkeyEvent(reg_key.key, reg_key.modifier));
	}
}

/*
uint32_t WinNative::GetKeyFrom(uint32_t key, uint32_t type)
{
	// TODO: support all the keys...
	static bool map_inited = false;
	static std::unordered_map<uint32_t, uint32_t> vk_key_map;
	static std::unordered_map<uint32_t, uint32_t> glfw_key_map =
	{
		{ MapVirtualKey(VK_LCONTROL    , 0)            , GLFW_KEY_LEFT_CONTROL    },
		{ MapVirtualKey(VK_RCONTROL    , 0)            , GLFW_KEY_RIGHT_CONTROL   },
		{ MapVirtualKey(VK_LMENU       , 0)            , GLFW_KEY_LEFT_ALT        },
		{ MapVirtualKey(VK_RMENU       , 0)            , GLFW_KEY_RIGHT_ALT       },
		{ MapVirtualKey(VK_LSHIFT      , 0)            , GLFW_KEY_LEFT_SHIFT      },
		{ MapVirtualKey(VK_RSHIFT      , 0)            , GLFW_KEY_RIGHT_SHIFT     },
		{ MapVirtualKey(VK_LWIN        , 0)            , GLFW_KEY_LEFT_SUPER      },
		{ MapVirtualKey(VK_RWIN        , 0)            , GLFW_KEY_RIGHT_SUPER     },
		{ MapVirtualKey(VK_LEFT        , 0)            , GLFW_KEY_LEFT            },
		{ MapVirtualKey(VK_RIGHT       , 0)            , GLFW_KEY_RIGHT           },
		{ MapVirtualKey(VK_UP          , 0)            , GLFW_KEY_UP              },
		{ MapVirtualKey(VK_DOWN        , 0)            , GLFW_KEY_DOWN            },
		{ MapVirtualKey(VK_SPACE       , 0)            , GLFW_KEY_SPACE           },
		{ MapVirtualKey(VK_OEM_COMMA   , 0)            , GLFW_KEY_COMMA           },
		{ MapVirtualKey(VK_OEM_MINUS   , 0)            , GLFW_KEY_MINUS           },
		{ MapVirtualKey(VK_OEM_PLUS    , 0)            , GLFW_KEY_EQUAL           },
		{ MapVirtualKey(VK_OEM_PERIOD  , 0)            , GLFW_KEY_PERIOD          },
		{ MapVirtualKey(VK_OEM_1       , 0)            , GLFW_KEY_SEMICOLON       },
		{ MapVirtualKey(VK_OEM_2       , 0)            , GLFW_KEY_SLASH           },
		{ MapVirtualKey(VK_OEM_3       , 0)            , GLFW_KEY_GRAVE_ACCENT    },
		{ MapVirtualKey(VK_OEM_4       , 0)            , GLFW_KEY_LEFT_BRACKET    },
		{ MapVirtualKey(VK_OEM_5       , 0)            , GLFW_KEY_BACKSLASH       },
		{ MapVirtualKey(VK_OEM_6       , 0)            , GLFW_KEY_RIGHT_BRACKET   },
		{ MapVirtualKey(VK_OEM_7       , 0)            , GLFW_KEY_APOSTROPHE      }
	};
	if (!map_inited)
	{
		// number keys..
		for (auto num_pad_key = VK_NUMPAD0, glfw_num = GLFW_KEY_KP_0;     num_pad_key <= VK_NUMPAD9; num_pad_key++, glfw_num++)
			glfw_key_map[MapVirtualKey(num_pad_key, 0)]                = glfw_num;
		for (WCHAR num_key = '0',           glfw_num = GLFW_KEY_0;        num_key <= '9';            num_key++, glfw_num++)
			glfw_key_map[MapVirtualKey(LOBYTE(VkKeyScan(num_key)), 0)] = glfw_num;
		// f keys..
		for (auto f_key = VK_F1,            glfw_f = GLFW_KEY_F1;         f_key <= VK_F24;           f_key++, glfw_f++)
			glfw_key_map[MapVirtualKey(f_key, 0)]                      = glfw_f;
		// chars
		for (WCHAR a_l = 'a',               glfw_al = GLFW_KEY_A;         a_l <= 'z';                a_l++, glfw_al++)
			glfw_key_map[MapVirtualKey(LOBYTE(VkKeyScan(a_l)), 0)]     = glfw_al;
		for (WCHAR a_c = 'A',               glfw_ac = GLFW_KEY_A;         a_c <= 'Z';                a_c++, glfw_ac++)
			glfw_key_map[MapVirtualKey(LOBYTE(VkKeyScan(a_c)), 0)]     = glfw_ac;

		for (auto& it : glfw_key_map)
		{
			vk_key_map[it.second] = MapVirtualKey(it.first, MAPVK_VSC_TO_VK);
		}

		map_inited = true;
	}
	if (type == 0 && glfw_key_map[key])
		return glfw_key_map[key];
	else if (vk_key_map[key])
		return vk_key_map[key];

	return 0;
}

bool WinNative::OnKeyPress(KBDLLHOOKSTRUCT* info)
{
	if (last_key_.is_down)
	{
		auto glfw_modifier = GetGLFWKeyFrom(last_key_.key);
		auto glfw_key = GetGLFWKeyFrom(info->vkCode);
		auto handled = 0u;
		auto found_it = registered_keys_.cend();

		auto checker = [](uint32_t modifier, uint32_t key, const std::unordered_map<std::string, RegKey>& map)
		{
			std::string key_combo = std::to_string(modifier) + "+" + std::to_string(key);
			return map.find(key_combo);
		};
	CHECK:
		if (handled++ > 2)
			return false;
		found_it = checker(glfw_modifier, glfw_key, registered_keys_);

		if (found_it == registered_keys_.cend())
		{
			glfw_key = GetGLFWKeyFrom(info->scanCode);
			goto CHECK;
		}

		EventBus::Instance().publish(new HotkeyEvent(glfw_key, glfw_modifier, Application::GetTotalRunningTime()));
		return true;
	}
	else if (last_key_.key == 0)
	{
		for (auto& r_it : registered_keys_)
		{
			auto& reg_key = r_it.second;
			if (reg_key.modifier == 0 && (reg_key.key == info->vkCode || reg_key.key == info->scanCode))
			{
				EventBus::Instance().publish(new HotkeyEvent(GetGLFWKeyFrom(info->vkCode), 0, Application::GetTotalRunningTime()));
				return true;
			}
			else if (reg_key.modifier == info->vkCode || reg_key.modifier == info->scanCode)
			{
				last_key_ = { info->vkCode, true, static_cast<float>(info->time) };
				return false;
			}
		}
	}
	return false;
}

bool WinNative::OnKeyRelease(KBDLLHOOKSTRUCT* info)
{
	if (last_key_.is_down && info->vkCode == last_key_.key)
		last_key_ = {};
	return false;
}

LRESULT WinNative::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		auto KBDLLHOOK = ((KBDLLHOOKSTRUCT*)lParam);
		if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN || wParam == WM_CHAR)
		{
			if (instance_->OnKeyPress(KBDLLHOOK))
				return 1;
		}
		else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
		{
			if (instance_->OnKeyRelease(KBDLLHOOK))
				return 1;
		}
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
*/

LRESULT WinNative::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_HOTKEY:
		if (wParam != IDHOT_SNAPDESKTOP && wParam != IDHOT_SNAPWINDOW)
		{
			instance_->OnHotkey(static_cast<uint32_t>(wParam));
		}
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT WinNative::LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION)
	{
		auto mouse_hook = ((MOUSEHOOKSTRUCT*)lParam);
		int x = mouse_hook->pt.x;
		int y = mouse_hook->pt.y;
		switch (wParam)
		{
		case WM_LBUTTONDOWN:
			EventBus::Instance().publish(new MouseButtonEvent(MOUSE_LBUTTON, true, x, y));
			break;
		case WM_LBUTTONUP:
			EventBus::Instance().publish(new MouseButtonEvent(MOUSE_LBUTTON, false, x, y));
			break;
		case WM_RBUTTONDOWN:
			EventBus::Instance().publish(new MouseButtonEvent(MOUSE_RBUTTON, true, x, y));
			break;
		case WM_RBUTTONUP:
			EventBus::Instance().publish(new MouseButtonEvent(MOUSE_RBUTTON, false, x, y));
			break;
		case WM_MBUTTONDOWN:
			EventBus::Instance().publish(new MouseButtonEvent(MOUSE_MBUTTON, true, x, y));
			break;
		case WM_MBUTTONUP:
			EventBus::Instance().publish(new MouseButtonEvent(MOUSE_MBUTTON, false, x, y));
			break;
		}
	}
	return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
