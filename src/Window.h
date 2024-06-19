#pragma once

#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <functional>
#include <Windows.h>

#include "KeyCode.h"
#include "Timer.h"
#include "event/IEventListener.h"
#include "imgui/ImGuiManager.h"

class Window : public IEventListener {
public:
	Window(uint16_t width, uint16_t height, const char* windowName, ImGuiManager& imguiManager);
	virtual ~Window();

	int ProcessMessages();
	float GetDeltaTime();
	bool IsKeyPressed(KeyCode key) const;

	void* CreateVulkanSurface(void* instance, void* allocator) const;
	const char** GetVulkanRequiredExtensions(uint32_t* count) const;
	void* GetWindowHandle() const;
	
	void OnEvent(EventCode code, const Event& event) override;
	
private:
	static LRESULT CALLBACK WindowMessageThunk(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK ProcessWindowMessages(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	[[nodiscard]] const char* TranslateErrorMessage(DWORD error) const;
	void RegisterRawMouseInput() const;
	void RegisterForEvents();
	void HideCursor();
	void ShowCursor();
	void CloseWindow();
	
private:
	uint16_t m_Width;
	uint16_t m_Height;
	HMODULE m_Instance;
	const char* m_WindowName;
	HWND m_WindowHandle;
	bool m_KeyState[static_cast<int>(KeyCode::MAX)]{};
	Timer m_Timer;
	BOOL m_CursorEnabled = TRUE;
	static constexpr LPCSTR m_ClassName = "StimplyWindow";
};

