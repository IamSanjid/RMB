#pragma once

#include <string>
#include <memory>

struct GLFWwindow;
struct HotkeyEvent;
class MainView;
class Mouse;
class Controller;

class Application
{
public:
	Application();

	Application(const Application&) = delete;
	Application& operator=(const Application&) = delete;

	~Application();

	static std::unique_ptr<Application> Create();

	static Application* GetInstance();
	static double GetTotalRunningTime();

	bool Initialize(const char* name, uint32_t width, uint32_t height);

	void Run();
	void Reconfig();
	void TogglePanning();

	bool IsPanning() { return panning_started_; };
private:
	void StartPanning();
	void OnHotkey(HotkeyEvent* evt);
	static void OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	static Application* instance_;

	GLFWwindow* main_window_ = nullptr;
	MainView* main_view_ = nullptr;
	Mouse* mouse_;
	Controller* controller_;

	bool is_running_ = false;
	bool panning_started_ = false;
};
