#include "Engine.h"

#include "Logger.h"
#include "Mesh.h"
#include "Scene.h"
#include "event/EventManager.h"

Engine::Engine(uint16_t width, uint16_t height)
	:
	m_Window(width, height, "Stimply Engine", m_ImGuiManager),
	m_Renderer(width, height, &m_Window, m_ImGuiManager)
{
	m_Mesh = new Scene("./Models/Sponza/sponza.obj");
	
	Logger::Debug("Scene created!\n");
	m_Renderer.AddScene(m_Mesh);
	EventManager::RegisterListener(EVENT_KEY_PRESSED, this);
}

Engine::~Engine() {
	delete m_Mesh;
	m_ImGuiManager.Shutdown();
	EventManager::ClearAllListeners();
}

void Engine::Run() {
	float endTime = 0.0f;

	Event event;
	EventManager::FireEvent(EVENT_HIDE_CURSOR, event);
	
	while (m_Window.ProcessMessages()) {
		float deltaTime = m_Window.GetDeltaTime();
		m_Renderer.BeginFrame(deltaTime);
		m_Renderer.RenderFrame(deltaTime);
		m_Renderer.EndFrame();
	}
}

void Engine::OnEvent(EventCode code, const Event& event)
{
	if (code == EVENT_KEY_PRESSED)
	{
		if (event.context.u32[0] == (int)KeyCode::Key_H)
		{
			if (!m_ShowingMouse)
			{
				// re-use event, since EVENT_SHOW_CURSOR doesn't expect any context...
				EventManager::FireEvent(EVENT_SHOW_CURSOR, event);
				m_ShowingMouse = true;
			}
			else
			{
				// re-use event, since EVENT_SHOW_CURSOR doesn't expect any context...
				EventManager::FireEvent(EVENT_HIDE_CURSOR, event);
				m_ShowingMouse = false;
			}
		}
		if (event.context.u32[0] == (int)KeyCode::Escape)
		{
			// re-use event, since EVENT_WINDOW_CLOSE doesn't expect any context...
			EventManager::FireEvent(EVENT_WINDOW_CLOSE, event);
		}
	}	
}
