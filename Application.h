#pragma once

#include <string>
#include <memory>

struct GLFWwindow;
struct HotkeyEvent;
struct MouseButtonEvent;
class MainView;
class Mouse;
class NpadController;
class Config;

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
	static NpadController* GetController();

	bool Initialize(const char* name, uint32_t width, uint32_t height);

	void Run();
	void Reconfig(Config* new_conf = nullptr);
	void TogglePanning();

	bool IsPanning() const { return panning_started_; }

private:
	void DetectMouseMove();
	void OnHotkey(HotkeyEvent* evt);
	void OnMouseButton(MouseButtonEvent* evt);
	void OnMouseMove(int x, int y);
	void UpdateMouseVisibility(double new_moved_time = 0.0);

	static void OnKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

	static Application* instance_;

	GLFWwindow* main_window_ = nullptr;
	MainView* main_view_ = nullptr;
	Mouse* mouse_;
	NpadController* controller_;

	bool is_running_ = false;
	bool panning_started_ = false;
};
