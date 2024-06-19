#pragma once
#include "../event/IEventListener.h"

struct ImGuiIO;
class Window;
struct ImGuiContext;
struct ImGui_ImplVulkan_InitInfo;
class Renderer;

class ImGuiManager : public IEventListener
{
public:
    ImGuiManager();
    ~ImGuiManager() override = default;
    
    void Shutdown();
    void InitializeWin32(void* window);
    void InitializeVulkan(ImGui_ImplVulkan_InitInfo* initInfo);
    void NewFrame();
    void Draw(Renderer* renderer, float deltaTime) const;
    void EndFrame(void* commandBuffer);
    bool ShouldDispatchMessage() const;

    void OnEvent(EventCode code, const Event& event) override;

private:
    Window*         m_Window;
    ImGuiContext*   m_ImguiContext;
    ImGuiIO*        m_Io;
};
