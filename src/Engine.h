#pragma once

#include "Window.h"
#include "Renderer.h"
#include "event/IEventListener.h"

class Engine : public IEventListener {
public:
	Engine(uint16_t width, uint16_t height);
	virtual ~Engine();
	void Run();

	void OnEvent(EventCode code, const Event& event) override;

private:
	ImGuiManager m_ImGuiManager;
	Window m_Window;
	Renderer m_Renderer;
	class Scene* m_Mesh;
	bool m_ShowingMouse = false;
};

