#include "Window.h"

#include <Windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#include "Logger.h"
#include "event/EventManager.h"
#include "exception/WindowException.h"
#include "imgui/lib/imgui.h"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window::Window(uint16_t width, uint16_t height, const char* windowName, ImGuiManager& imguiManager)
	:
	m_Width(width),
	m_Height(height),
	m_Instance(GetModuleHandle(0)),
	m_WindowName(windowName)
{
	WNDCLASSEXA wndClass;
	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_VREDRAW | CS_HREDRAW;
	wndClass.lpfnWndProc = WindowMessageThunk;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = m_Instance;
	wndClass.hIcon = nullptr;
	wndClass.hCursor = nullptr;
	wndClass.hbrBackground = nullptr;
	wndClass.lpszMenuName = nullptr;
	wndClass.lpszClassName = m_ClassName;
	wndClass.hIconSm = nullptr;
	
	ATOM classRes = RegisterClassExA(&wndClass);
	if (!classRes)
	{
		throw WindowException(TranslateErrorMessage(GetLastError()));
	}

	DWORD exStyles = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	DWORD styles = WS_OVERLAPPEDWINDOW;

	RECT rect{};
	rect.right = m_Width;
	rect.bottom = m_Height;
	
	AdjustWindowRectEx(&rect, styles, FALSE, exStyles);

	RECT adjustedRect{};
	adjustedRect.right = rect.right - rect.left;
	adjustedRect.bottom = rect.bottom - rect.top;
	
	HWND hwnd = CreateWindowExA(
		exStyles,
		m_ClassName,
		m_WindowName,
		styles,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		adjustedRect.right,
		adjustedRect.bottom,
		nullptr,
		nullptr,
		m_Instance,
		static_cast<LPVOID>(this)
	);

	if (!hwnd)
	{
		throw WindowException(TranslateErrorMessage(GetLastError()));
	}

	ShowWindow(hwnd, SW_SHOW);

	m_WindowHandle = hwnd;
	imguiManager.InitializeWin32(m_WindowHandle);

	RegisterRawMouseInput();
	RegisterForEvents();
}

Window::~Window() {
	if (m_WindowHandle) DestroyWindow(m_WindowHandle);
	UnregisterClassA(m_ClassName, m_Instance);
}

int Window::ProcessMessages() {
	MSG msg;

	while (PeekMessageA(&msg, m_WindowHandle, NULL, NULL, PM_REMOVE) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
		if (msg.message == WM_QUIT)
		{
			return 0;
		}
	}

	m_Timer.Tick();

	return 1;
}

void Window::CloseWindow() {
	DestroyWindow(m_WindowHandle);
	m_WindowHandle = nullptr;
}

float Window::GetDeltaTime() {
	return m_Timer.GetDeltaTime();
}

bool Window::IsKeyPressed(KeyCode key) const
{
	return m_KeyState[(int)key];
}

/**
 * 
 * @param count when count == 0, this function will set in count how much extensions there are required, when count > 0, this function will use it to add the extensions in the array.
 * @return null if count == 0 or a dynamically allocated array of required extensions. the user needs to free the memory after usage with delete[].
 */
const char** Window::GetVulkanRequiredExtensions(uint32_t* count) const {
	if (*count == 0)
	{
		*count = 2;
		return nullptr;
	}
	static const char* surface = VK_KHR_SURFACE_EXTENSION_NAME;
	static const char* winsurface = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
	const char** arr = new const char*[2];
	arr[0] = surface;
	arr[1] = winsurface;
	return arr;
}

void* Window::GetWindowHandle() const
{
	return m_WindowHandle;
}

void Window::HideCursor()
{
	RECT winRect{};
	GetWindowRect(m_WindowHandle, &winRect);

	ClipCursor(&winRect);

	m_CursorEnabled = false;
	while (::ShowCursor(m_CursorEnabled) >= 0);
}

void Window::ShowCursor()
{
	ClipCursor(nullptr);
	m_CursorEnabled = true;
	while (::ShowCursor(m_CursorEnabled) < 0);
}

void Window::OnEvent(EventCode code, const Event& event)
{
	switch (code)
	{
	case EVENT_WINDOW_CLOSE:
		Logger::Debug("EVENT_WINDOW_CLOSE received...\n");
		CloseWindow();
		break;
	case EVENT_SHOW_CURSOR:
		ShowCursor();
		break;
	case EVENT_HIDE_CURSOR:
		HideCursor();
		break;
	default: break;
	}
}

LRESULT Window::WindowMessageThunk(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_NCCREATE)
	{
		CREATESTRUCTA* cs = reinterpret_cast<CREATESTRUCTA*>(lParam);
		LONG_PTR window = reinterpret_cast<LONG_PTR>(cs->lpCreateParams);
		SetWindowLongPtrA(hWnd, GWLP_USERDATA, window);
	}
	
	Window* window = reinterpret_cast<Window*>(GetWindowLongPtrA(hWnd, GWLP_USERDATA));
	if (!window)
	{
		return DefWindowProcA(hWnd, message, wParam, lParam);
	}
	return window->ProcessWindowMessages(hWnd, message, wParam, lParam);
}

LRESULT Window::ProcessWindowMessages(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;
	
	ImGuiIO& io = ImGui::GetIO();
	if (io.WantCaptureMouse || io.WantCaptureKeyboard)
		return true;
	
	switch (message)
	{
	// ************************ keyboard begin ************************
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		// https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#keystroke-message-flags
		WORD keyCode = LOWORD(wParam);
		WORD keyFlags = HIWORD(lParam);
		BOOL isKeyUp = (keyFlags & KF_UP) == KF_UP;

		m_KeyState[keyCode] = !isKeyUp;

		Event event{};
		event.sender = this;
		event.context.u32[0] = keyCode;
		if (!isKeyUp)	
			EventManager::FireEvent(EventCode::EVENT_KEY_PRESSED, event);
		else
			EventManager::FireEvent(EventCode::EVENT_KEY_RELEASED, event);
		//Logger::Debug("Key %d %s\n", keyCode, isKeyReleased ? "Up" : "Down");
		break;	
	}
	// ************************ keyboard end ************************
	// ************************ mouse start ************************
	case WM_INPUT:
		{
			UINT dataSize;
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dataSize, sizeof(RAWINPUTHEADER));

			RAWINPUT* buffer = (RAWINPUT*)alloca(dataSize);
			memset(buffer, 0, dataSize);
			UINT bytesRead = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &dataSize, sizeof(RAWINPUTHEADER));
		
			if (bytesRead != dataSize)
			{
				Logger::Error("WM_INPUT: bytesRead are not the same size as dataSize.\n");		
			}

			int64_t x = buffer->data.mouse.lLastX;
			int64_t y = buffer->data.mouse.lLastY;

			Event event{};
			event.sender = this;
			event.context.i64[0] = x;
			event.context.i64[1] = y;
			
			EventManager::FireEvent(EventCode::EVENT_MOUSE_MOVED, event);
			
			break;	
		}		
	// ************************ mouse end ************************	
	// ************************ end messages ************************
	case WM_CLOSE:
		EventManager::FireEvent(EVENT_WINDOW_CLOSE, {});
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProcA(hWnd, message, wParam, lParam);
	}
	return DefWindowProcA(hWnd, message, wParam, lParam);
}

const char* Window::TranslateErrorMessage(DWORD error) const
{
	// max message size is 128K
	// refer: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-formatmessage
	char* message = new char[128000];
	memset(message, 0, 128000);
	DWORD msgSize = FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM,
		m_Instance,
		error,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
		message,
		128000 - 1,
		nullptr
	);

	// throwing an exception with this message will cause a memory leak
	// but since the application will end anyway, I am not fixing this.
	return message;
}

void Window::RegisterRawMouseInput() const
{
	RAWINPUTDEVICE rawInputDevice;
	rawInputDevice.usUsagePage = 1;
	rawInputDevice.usUsage = 2;
	rawInputDevice.dwFlags = 0;
	rawInputDevice.hwndTarget = m_WindowHandle;

	if (RegisterRawInputDevices(&rawInputDevice, 1, sizeof(rawInputDevice)) == 0)
	{
		throw WindowException(TranslateErrorMessage(GetLastError()));
	}
}

void Window::RegisterForEvents()
{
	EventManager::RegisterListener(EVENT_WINDOW_CLOSE, this);
	EventManager::RegisterListener(EVENT_SHOW_CURSOR, this);
	EventManager::RegisterListener(EVENT_HIDE_CURSOR, this);
}

void* Window::CreateVulkanSurface(void* instance, void* allocator) const {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkWin32SurfaceCreateInfoKHR createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.hinstance = m_Instance;
	createInfo.hwnd = m_WindowHandle;
	
	vkCreateWin32SurfaceKHR((VkInstance)instance, &createInfo, (VkAllocationCallbacks*)allocator, &surface);
	return (void*)surface;
}
