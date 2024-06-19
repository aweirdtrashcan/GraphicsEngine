#include "ImGuiManager.h"

#include "lib/imgui.h"
#include "lib/imgui_impl_vulkan.h"
#include "lib/imgui_impl_win32.h"
#include "../Renderer.h"
#include "../Window.h"
#include "../event/EventManager.h"
#include "../exception/ImGuiManagerException.h"

ImGuiManager::ImGuiManager()
    :
    m_Window(nullptr),
    m_Io(nullptr)
{
    IMGUI_CHECKVERSION();
    m_ImguiContext = ImGui::CreateContext();
    EventManager::RegisterListener(EVENT_HIDE_CURSOR, this);
    EventManager::RegisterListener(EVENT_SHOW_CURSOR, this);
}

void ImGuiManager::Shutdown()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(m_ImguiContext);
    m_ImguiContext = nullptr;
}

void ImGuiManager::InitializeWin32(void* window)
{
    if (!ImGui_ImplWin32_Init(window))
    {
        throw ImGuiManagerException("Failed to initialize win32 implementation!");
    }
}

void ImGuiManager::InitializeVulkan(ImGui_ImplVulkan_InitInfo* initInfo)
{
    if (!ImGui_ImplVulkan_Init(initInfo))
    {
        throw ImGuiManagerException("Failed to initialize vulkan implementation!");
    }
    m_Io = &ImGui::GetIO();
}

void ImGuiManager::NewFrame()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Draw(Renderer* renderer, float deltaTime) const
{
    if (ImGui::Begin("Test window"))
    {
        ImGui::Text("FPS: %d", (int)(1.0f / deltaTime));
        ImGui::Text("DeltaTime: %f", deltaTime);
    }
    ImGui::End();

    if (ImGui::Begin("Mouse"))
    {
        ImGui::Text("Mouse pos: %f %f", renderer->m_CameraPitch, renderer->m_CameraYaw);
    }
    ImGui::End();
}

void ImGuiManager::EndFrame(void* commandBuffer)
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)commandBuffer);
}

bool ImGuiManager::ShouldDispatchMessage() const
{
    return !m_Io->WantCaptureKeyboard || !m_Io->WantCaptureMouse;
}

void ImGuiManager::OnEvent(EventCode code, const Event& event)
{
    switch (code)
    {
    case EVENT_HIDE_CURSOR:
        m_Io->ConfigFlags |= ImGuiConfigFlags_NoMouse;
        break;
    case EVENT_SHOW_CURSOR:
        m_Io->ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    default: break;
    }
}
