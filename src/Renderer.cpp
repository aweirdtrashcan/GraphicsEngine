#include "Renderer.h"

#include <fstream>
#include <iostream>

#define _AMD64_
#include <ConsoleApi2.h>
#include <WinBase.h>
#include <vulkan/vulkan.h>

#include "Logger.h"
#include "Window.h"
#include "imgui/lib/imgui_impl_vulkan.h"
#include "exception/RendererException.h"
#include "Scene.h"
#include "event/EventManager.h"

//#define SHOW_EXTRA_INFO

Renderer::Renderer(uint16_t width, uint16_t height, Window* window, ImGuiManager& imguiManager)
	:
	m_ImGuiManager(imguiManager)
{
	assert(s_RendererInstance == nullptr);
	m_Instance = CreateVulkanInstance();
	m_Window = window;

	m_DebugMessenger = CreateDebugMessenger(m_Instance);

	VkPhysicalDeviceFeatures features{};
	features.fillModeNonSolid = VK_TRUE;
	features.samplerAnisotropy = VK_TRUE;
	features.sampleRateShading = VK_TRUE;
	features.depthClamp = VK_TRUE;

	m_PhysicalDevice = PickPhysicalDevice(m_Instance, features);
	m_PhysicalDeviceInfo = GetPhysicalDeviceInfo(m_PhysicalDevice);

	m_Surface = (VkSurfaceKHR)m_Window->CreateVulkanSurface(m_Instance, m_Allocator);

	std::vector<OptionalVulkanRequest> desiredExtensions;
	desiredExtensions.push_back({ "VK_KHR_swapchain", true });
	desiredExtensions.push_back({ "VK_KHR_maintenance1", true });
	m_LogicalDevice = CreateLogicalDevice(
		m_PhysicalDevice, 
		desiredExtensions, 
		features, 
		m_GraphicsQueue, 
		m_TransferQueue, 
		m_GraphicsQueueIndex,
		m_TransferQueueIndex
	);
	if (m_GraphicsQueueIndex == m_TransferQueueIndex) {
		m_PhysicalDeviceInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		m_PhysicalDeviceInfo.queueFamilyIndices[0] = m_GraphicsQueueIndex;
		m_PhysicalDeviceInfo.queueFamilyCount = 1;
	} else {
		m_PhysicalDeviceInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		m_PhysicalDeviceInfo.queueFamilyIndices[0] = m_GraphicsQueueIndex;
		m_PhysicalDeviceInfo.queueFamilyIndices[1] = m_TransferQueueIndex;
		m_PhysicalDeviceInfo.queueFamilyCount = 2;
	}
	m_Swapchain = CreateSwapchain(m_PhysicalDevice, m_LogicalDevice, m_Surface, m_BackBuffers);
	m_Framecount = (uint32_t)m_BackBuffers.size();
	CreateSynchronizationObjects(m_Framecount, &m_GraphicsImageAcquiredSemaphores, &m_GraphicsQueueSubmittedSemaphore, &m_GraphicsFences);
	CreateSynchronizationObjects(m_Framecount, nullptr, nullptr, &m_TransferFences);
	m_DepthBuffer = CreateDepthBuffer(m_Width, m_Height, VK_SAMPLE_COUNT_1_BIT);
	m_RenderPass = CreateRenderPass(m_BackBuffers[0].format.format, m_DepthBuffer.format.format);
	m_Framebuffer = CreateFramebuffers(m_BackBuffers, m_DepthBuffer, m_RenderPass);
	VkDescriptorType types[] =
	{
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	};
	m_DescriptorPool= CreateDescriptorPool(types, _countof(types), 1000, 10000, true);
	VkDescriptorType uboTypes[] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
	VkDescriptorType cisTypes[] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER };
	m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_VERTEX_UNIFORM] = CreateDescriptorSetLayout(uboTypes, _countof(uboTypes), 1, VK_SHADER_STAGE_VERTEX_BIT);
	m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_FRAGMENT_UNIFORM] = CreateDescriptorSetLayout(uboTypes, _countof(uboTypes), 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_FRAGMENT_UNIFORM_BUFFER_COMBINED_IMAGE_SAMPLER] =
		CreateDescriptorSetLayout(cisTypes, _countof(cisTypes), 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_GraphicsCommandPool = CreateCommandPool(true, false, m_GraphicsQueueIndex);
	m_TransferCommandPool = CreateCommandPool(true, true, m_TransferQueueIndex);
	CreateGraphicsCommandBuffers(m_CommandBuffers);

	Shader shaders[2];
	shaders[0] = CreateShader("./Shaders/vertexshader.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaders[1] = CreateShader("./Shaders/fragmentshader.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorSetLayout layoutMvpLightTexture[] = { m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_VERTEX_UNIFORM], m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_FRAGMENT_UNIFORM_BUFFER_COMBINED_IMAGE_SAMPLER] };
	VkDescriptorSetLayout layoutMvpLight[] = { m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_VERTEX_UNIFORM], m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_FRAGMENT_UNIFORM] };
	m_PipelineLayouts[GRAPHICS_PIPELINE_TYPE_MVP_LIGHT_TEXTURE] = CreatePipelineLayout(_countof(layoutMvpLightTexture), layoutMvpLightTexture);
	m_PipelineLayouts[GRAPHICS_PIPELINE_TYPE_MVP_LIGHT] = CreatePipelineLayout(_countof(layoutMvpLight), layoutMvpLight);
	m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_MVP_LIGHT_TEXTURE] = CreateGraphicsPipeline(shaders, _countof(shaders), m_PipelineLayouts[GRAPHICS_PIPELINE_TYPE_MVP_LIGHT_TEXTURE], m_RenderPass);
	//m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_MVP_LIGHT] = CreateGraphicsPipeline(shaders, _countof(shaders), m_PipelineLayouts[GRAPHICS_PIPELINE_TYPE_MVP_LIGHT], m_RenderPass);

	vkDestroyShaderModule(m_LogicalDevice, shaders[0].shader, m_Allocator);
	vkDestroyShaderModule(m_LogicalDevice, shaders[1].shader, m_Allocator);

	CreateVertexBuffer();
	CreateIndexBuffer();

	UpdateVPBuffer(0.0f);

	s_RendererInstance = this;

	ImGui_ImplVulkan_InitInfo initInfo = GetVulkanImGuiInitInfo();
	m_ImGuiManager.InitializeVulkan(&initInfo);

	RegisterForEvents();

	Light centralLight;
	centralLight.m_Position = { 0.0f, 28.0f, 0.0f };
	centralLight.linearFalloff = 0.08f;
	centralLight.quadraticFalloff = 0.01f;
	m_WorldLights.push_back(centralLight);

	m_Sampler = CreateSampler();
}

Renderer::~Renderer() {
	vkDeviceWaitIdle(m_LogicalDevice);
	vkDestroySampler(m_LogicalDevice, m_Sampler, m_Allocator);
	for (VkPipeline pipeline : m_GraphicsPipelines)
		vkDestroyPipeline(m_LogicalDevice, pipeline, m_Allocator);
	for (VkPipelineLayout pipelineLayout : m_PipelineLayouts)
		vkDestroyPipelineLayout(m_LogicalDevice, pipelineLayout, m_Allocator);
	DestroyBuffer(m_IndexBuffer);
	DestroyBuffer(m_VertexBuffer);
	DestroyGraphicsCommandBuffers(m_CommandBuffers);
	vkDestroyCommandPool(m_LogicalDevice, m_TransferCommandPool, m_Allocator);
	vkDestroyCommandPool(m_LogicalDevice, m_GraphicsCommandPool, m_Allocator);
	for (VkDescriptorSetLayout setLayouts : m_DescriptorSetLayouts)
		vkDestroyDescriptorSetLayout(m_LogicalDevice, setLayouts, m_Allocator);
	vkDestroyDescriptorPool(m_LogicalDevice, m_DescriptorPool, m_Allocator);
	DestroyDepthBuffer(m_DepthBuffer);
	DestroyFramebuffers(m_Framebuffer);
	vkDestroyRenderPass(m_LogicalDevice, m_RenderPass, m_Allocator);
	DestroySynchronizationObjects(&m_GraphicsImageAcquiredSemaphores, &m_GraphicsQueueSubmittedSemaphore, &m_GraphicsFences);
	DestroySynchronizationObjects(nullptr, nullptr, &m_TransferFences);
	DestroyImageViews(m_BackBuffers);
	vkDestroySwapchainKHR(m_LogicalDevice, m_Swapchain, m_Allocator);
	vkDestroyDevice(m_LogicalDevice, m_Allocator);
	vkDestroySurfaceKHR(m_Instance, m_Surface, m_Allocator);
	DestroyDebugUtils(m_Instance, m_DebugMessenger);
	vkDestroyInstance(m_Instance, m_Allocator);
}

bool Renderer::BeginFrame(float deltaTime) {
	// initial setup
	if (m_Framebuffer.empty()) {
		RecreateSwapchain();
		return false;
	}

	vkWaitForFences(m_LogicalDevice, 1, &m_GraphicsFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

	m_CurrCmdBuf = m_CommandBuffers[m_CurrentFrame];
	m_CurrFence = m_GraphicsFences[m_CurrentFrame];
	m_CurrImgAcq = m_GraphicsImageAcquiredSemaphores[m_CurrentFrame];
	m_CurrQueueSubmt = m_GraphicsQueueSubmittedSemaphore[m_CurrentFrame];
	m_CurrFb = m_Framebuffer[m_CurrentFrame];

	VkResult result = vkAcquireNextImageKHR(
		m_LogicalDevice,
		m_Swapchain,
		UINT64_MAX,
		m_CurrImgAcq,
		VK_NULL_HANDLE,
		&m_FrameIndex
	);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		RecreateSwapchain();
		return false;
	}
	
	VkCommandBufferBeginInfo cmdBeginInfo;
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;
	cmdBeginInfo.flags = 0;
	cmdBeginInfo.pInheritanceInfo = nullptr;

	vkResetFences(m_LogicalDevice, 1, &m_CurrFence);
	vkResetCommandBuffer(m_CurrCmdBuf, 0);
	vkBeginCommandBuffer(m_CurrCmdBuf, &cmdBeginInfo);

	// color -> depth
	VkClearValue clearValues[2]{};
	clearValues[0].color.float32[0] = 0.7f;
	clearValues[0].color.float32[1] = 0.0f;
	clearValues[0].color.float32[2] = 0.7f;
	clearValues[0].color.float32[3] = 1.0f;
	clearValues[1].depthStencil.depth = 1.0f;

	VkRenderPassBeginInfo renderpassBeginInfo;
	renderpassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderpassBeginInfo.pNext = nullptr;
	renderpassBeginInfo.renderPass = m_RenderPass;
	renderpassBeginInfo.framebuffer = m_CurrFb;
	renderpassBeginInfo.renderArea.extent.width = m_Width;
	renderpassBeginInfo.renderArea.extent.height = m_Height;
	renderpassBeginInfo.renderArea.offset = { 0, 0 };
	renderpassBeginInfo.clearValueCount = _countof(clearValues);
	renderpassBeginInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(m_CurrCmdBuf, &renderpassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	
	VkViewport viewport;
	VkRect2D scissor;
	GetViewportAndScissor(viewport, scissor);

	vkCmdSetViewport(m_CurrCmdBuf, 0, 1, &viewport);
	vkCmdSetScissor(m_CurrCmdBuf, 0, 1, &scissor);

	CalculateAndShowFps(deltaTime);
	UpdateVPBuffer(deltaTime);
	
	m_ImGuiManager.NewFrame();

	for (size_t i = 0; i < m_WorldLights.size(); i++)
	{
		if (ImGui::Begin("Light"))
		{
			ImGui::SliderFloat3("Pos", &m_WorldLights[i].m_Position.x, -100.f, 100.f);
			ImGui::ColorEdit3("Light Color", &m_WorldLights[i].m_Color.x);
			ImGui::SliderFloat("Constant falloff", &m_WorldLights[i].constantFalloff, 0.0f, 2.0f);
			ImGui::SliderFloat("Linear falloff", &m_WorldLights[i].linearFalloff, 0.0f, 1.0f);
			ImGui::SliderFloat("Quadratic falloff", &m_WorldLights[i].quadraticFalloff, 0.0f, 0.1f);
		}
		ImGui::End();	
	}
	
	return true;
}

void Renderer::RenderFrame(float deltaTime) {
	vkCmdBindPipeline(m_CurrCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_MVP_LIGHT_TEXTURE]);

	// draw commands
	DrawAllMeshes(m_CurrCmdBuf, m_FrameIndex);
	m_ImGuiManager.Draw(this, deltaTime);
	// end draw
}

void Renderer::EndFrame()
{
	m_ImGuiManager.EndFrame(m_CurrCmdBuf);
	
	vkCmdEndRenderPass(m_CurrCmdBuf);
	vkEndCommandBuffer(m_CurrCmdBuf);

	VkPipelineStageFlags stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_CurrImgAcq;
	submitInfo.pWaitDstStageMask = stages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_CurrCmdBuf;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_CurrQueueSubmt;

	VkResult result = vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_CurrFence);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		RecreateSwapchain();
		return;
	}

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_CurrQueueSubmt;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.pImageIndices = &m_FrameIndex;
	presentInfo.pResults = VK_NULL_HANDLE;

	result = vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		RecreateSwapchain();
		return;
	}

	m_CurrentFrame = (m_CurrentFrame + 1) % m_Framecount;
}

void Renderer::AddScene(const class Scene* scene) {
	m_Meshes.push_back(scene);
}

VkFence Renderer::CreateFence() const
{
	VkFence fence;
	
	VkFenceCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;

	VkRes(vkCreateFence(m_LogicalDevice, &createInfo, m_Allocator, &fence), "Failed to create fence");

	return fence;
}

void Renderer::DestroyFence(VkFence fence) const
{
	vkDestroyFence(m_LogicalDevice, fence, m_Allocator);
}

const Renderer* Renderer::Get() {
	return s_RendererInstance;
}

void Renderer::OnEvent(EventCode code, const Event& event)
{
	switch (code)
	{
	case EVENT_MOUSE_MOVED:
		MouseMoved(event.context.i64[0], event.context.i64[1]);
		break;
	case EVENT_HIDE_CURSOR:
		m_MouseShowing = false;
		break;
	case EVENT_SHOW_CURSOR:
		m_MouseShowing = true;
		break;
	default: break;
	}
}

ImGui_ImplVulkan_InitInfo Renderer::GetVulkanImGuiInitInfo() const
{
	ImGui_ImplVulkan_InitInfo initInfo;
	initInfo.Instance = m_Instance;
	initInfo.PhysicalDevice = m_PhysicalDevice;
	initInfo.Device = m_LogicalDevice;
	initInfo.QueueFamily = m_GraphicsQueueIndex;
	initInfo.Queue = m_GraphicsQueue;
	initInfo.DescriptorPool = m_DescriptorPool;
	initInfo.RenderPass = m_RenderPass;
	initInfo.MinImageCount = m_Framecount;
	initInfo.ImageCount = m_Framecount;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.Subpass = 0;
	initInfo.UseDynamicRendering = false;
	initInfo.PipelineRenderingCreateInfo = {};
	initInfo.Allocator = nullptr;
	initInfo.CheckVkResultFn = nullptr;
	initInfo.MinAllocationSize = 1024 * 1024;

	return initInfo;
}

void Renderer::CopyBufferToImage(VkCommandBuffer commandBuffer, const GPUBuffer& buffer, const GPUImage& image) const
{
	VkBufferImageCopy copyRegion;
	copyRegion.bufferOffset = 0;
	copyRegion.bufferRowLength = 0;
	copyRegion.bufferImageHeight = 0;
	copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copyRegion.imageSubresource.layerCount = 1;
	copyRegion.imageSubresource.mipLevel = 0;
	copyRegion.imageSubresource.baseArrayLayer = 0;
	copyRegion.imageOffset = {};
	copyRegion.imageExtent.depth = 1.0f;
	copyRegion.imageExtent.width = static_cast<uint32_t>(image.width);
	copyRegion.imageExtent.height = static_cast<uint32_t>(image.height);
	
	vkCmdCopyBufferToImage(commandBuffer, buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
}

VkDebugUtilsMessengerCreateInfoEXT Renderer::GetDebugMessengerCreateInfo() {
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugCreateInfo.pNext = nullptr;
	debugCreateInfo.flags = 0;
	debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
	debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugCreateInfo.pfnUserCallback = debugUtilsMessenger;
	debugCreateInfo.pUserData = nullptr;

	return debugCreateInfo;
}

VkInstance Renderer::CreateVulkanInstance() const {
	VkApplicationInfo applicationInfo;
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pNext = nullptr;
	applicationInfo.pApplicationName = "Stimply Engine";
	applicationInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	applicationInfo.pEngineName = "Stimply Engine";
	applicationInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	applicationInfo.apiVersion = VK_MAKE_API_VERSION(0, 1, 3, 368);

	uint32_t propertiesCount;
	vkEnumerateInstanceLayerProperties(&propertiesCount, nullptr);
	std::vector<VkLayerProperties> layerProperties(propertiesCount);
	vkEnumerateInstanceLayerProperties(&propertiesCount, layerProperties.data());

#ifdef SHOW_EXTRA_INFO
	std::cout << "Available Instance Layer Properties:\n";
	for (VkLayerProperties& props : layerProperties) {
		std::cout << "\t " << props.layerName << "\n";
	}
#endif

	std::vector<OptionalVulkanRequest> desiredLayers;
	desiredLayers.push_back({ "VK_LAYER_RTSS", false });
#ifdef _DEBUG
	desiredLayers.push_back({ "VK_LAYER_KHRONOS_validation", true });
#endif
	std::vector<const char*> requestLayers;

	for (OptionalVulkanRequest& desiredLayer : desiredLayers) {
		bool found = false;
		for (VkLayerProperties& props : layerProperties) {
			if (strcmp(desiredLayer.name, props.layerName) == 0) {
				requestLayers.push_back(desiredLayer.name);
				found = true;
				break;
			}
		}

		if (!found && desiredLayer.required) {
			throw std::exception("One or more desired instance layers are not supported!");
		}
	}

	propertiesCount = 0;

	vkEnumerateInstanceExtensionProperties(nullptr, &propertiesCount, nullptr);
	std::vector<VkExtensionProperties> supportedExtensions(propertiesCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &propertiesCount, supportedExtensions.data());

#ifdef SHOW_EXTRA_INFO
	std::cout << "Available Instance Extensions:\n";
	for (VkExtensionProperties& props : supportedExtensions) {
		std::cout << "\t " << props.extensionName << "\n";
	}
#endif

	propertiesCount = 0;
	m_Window->GetVulkanRequiredExtensions(&propertiesCount);
	const char** _desiredExtensions = m_Window->GetVulkanRequiredExtensions(&propertiesCount);
	std::vector<const char*> desiredExtensions;

#ifdef _DEBUG
	desiredExtensions.push_back("VK_EXT_debug_utils");
#endif

	for (uint32_t i = 0; i < propertiesCount; i++) {
		bool supported = false;
		desiredExtensions.push_back(_desiredExtensions[i]);
		for (VkExtensionProperties& supportedExtension : supportedExtensions) {
			if (strcmp(desiredExtensions[i], supportedExtension.extensionName) == 0) {
				supported = true;
				break;
			}
		}
		if (!supported) {
			throw std::exception("One or more of the required instance extensions required by this application are not supported!");
		}
	}

	delete[] _desiredExtensions;
	
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = GetDebugMessengerCreateInfo();

	VkInstanceCreateInfo instanceCreateInfo;
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = &debugCreateInfo;
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;
	instanceCreateInfo.enabledLayerCount = (uint32_t)requestLayers.size();
	instanceCreateInfo.ppEnabledLayerNames = requestLayers.data();
	instanceCreateInfo.enabledExtensionCount = (uint32_t)desiredExtensions.size();
	instanceCreateInfo.ppEnabledExtensionNames = desiredExtensions.data();

	VkInstance instance;

	VkRes(vkCreateInstance(&instanceCreateInfo, m_Allocator, &instance), "Failed to create vulkan instance");

	return instance;
}

VkPhysicalDevice Renderer::PickPhysicalDevice(VkInstance instance, const VkPhysicalDeviceFeatures& desiredFeatures) const {
	uint32_t physicalDeviceCount = 0;
	VkRes(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr), "Failed to list physical devices!");
	
	std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
	VkRes(vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()), "Failed to list physical devices!");

	std::pair<VkPhysicalDevice, uint32_t> bestDevice = std::pair(nullptr, 0);

	for (VkPhysicalDevice physicalDevice : physicalDevices) {
		uint32_t devicePoints = RankPhysicalDevice(physicalDevice, desiredFeatures);
		if (devicePoints > bestDevice.second) {
			bestDevice = std::pair(physicalDevice, devicePoints);
		}
	}

	if (bestDevice.first == 0) {
		throw std::exception("Failed to find a device that supports all Vulkan requirements of this application.");
	}

	PhysicalDeviceInformation info = GetPhysicalDeviceInfo(bestDevice.first);

	double vramSize = 0;

	for (uint32_t i = 0; i < info.memoryProperties.memoryHeapCount; i++) {
		if ((info.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
			vramSize += info.memoryProperties.memoryHeaps[i].size;
		}
	}

	vramSize /= ToGigabyte;

	std::cout << "Selecting device " << info.properties.deviceName << "\n"
		<< "\tTYPE: " << (info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "Discrete GPU\n" : "Integrated GPU\n")
		<< "\tVRAM: " << vramSize << " GB\n";

	return bestDevice.first;
}

PhysicalDeviceInformation Renderer::GetPhysicalDeviceInfo(VkPhysicalDevice physicalDevice) const {
	PhysicalDeviceInformation info;

	vkGetPhysicalDeviceProperties(physicalDevice, &info.properties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &info.features);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &info.memoryProperties);

	return info;
}

uint32_t Renderer::RankPhysicalDevice(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceFeatures& desiredFeatures) const {
	uint32_t points = 0;

	PhysicalDeviceInformation info = GetPhysicalDeviceInfo(physicalDevice);

	uint32_t featureCount = sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);

	const VkBool32* pSupportedFeatures = (const VkBool32*)&info.features;
	const VkBool32* pDesiredFeatures = (const VkBool32*)&desiredFeatures;

	for (uint32_t i = 0; i < featureCount; i++) {
		if (pDesiredFeatures[i] == VK_TRUE) {
			if (!pSupportedFeatures[i]) {
				return 0;
			}
		}
	}

	if (info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		points += 4000;
	else if (info.properties.deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		points += 1000;

	size_t maxMemory = 0;

	for (uint32_t i = 0; i < info.memoryProperties.memoryHeapCount; i++) {
		if ((info.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
			maxMemory += info.memoryProperties.memoryHeaps[i].size;
		}
	}

	maxMemory = maxMemory / ToMegabyte;
	points += (uint32_t)maxMemory;

	return points;
}

VkDebugUtilsMessengerEXT Renderer::CreateDebugMessenger(VkInstance instance) const {
#ifdef _DEBUG
	VkDebugUtilsMessengerEXT debugMessenger;

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = GetDebugMessengerCreateInfo();

	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func) {
		func(instance, &debugCreateInfo, m_Allocator, &debugMessenger);
		return debugMessenger;
	}
#endif
	return VK_NULL_HANDLE;
}

void Renderer::DestroyDebugUtils(VkInstance instance, VkDebugUtilsMessengerEXT messenger) const {
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func) {
		func(instance, messenger, m_Allocator);
	}
}

VkDevice Renderer::CreateLogicalDevice(VkPhysicalDevice physicalDevice, const std::vector<OptionalVulkanRequest>& deviceExtensions,
									   const VkPhysicalDeviceFeatures& requestedFeatures, VkQueue& graphicsQueue, VkQueue& transferQueue,
									   uint32_t& graphicsQueueIndex, uint32_t& transferQueueIndex) const {
	VkDevice device;

	uint32_t supportedExtensionCount = 0;

	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, nullptr);
	std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, supportedExtensions.data());

#ifdef SHOW_EXTRA_INFO
	std::cout << "Available Device Extensions:\n";
	for (VkExtensionProperties& props : supportedExtensions) {
		std::cout << "\t " << props.extensionName << "\n";
	}
#endif

	std::vector<const char*> extensionsToRequest;

	for (const OptionalVulkanRequest& request : deviceExtensions) {
		bool supported = false;
		for (const VkExtensionProperties& supportedExtension : supportedExtensions) {
			if (strcmp(request.name, supportedExtension.extensionName) == 0) {
				supported = true;
				break;
			}
		}
		if (!supported && request.required) {
			std::cout << "Failed to validate support to " << request.name << " extension!\n";
			throw std::exception("One of the requested device extensions that are required are not supported!");
		}
		if (supported) {
			extensionsToRequest.push_back(request.name);
		}
	}

	QueueFamilyIndex indices = FindQueueFamilyIndices(m_PhysicalDevice);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	float priority = 1.0f;

	VkDeviceQueueCreateInfo graphicsQueueCreateInfo;
	graphicsQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	graphicsQueueCreateInfo.pNext = nullptr;
	graphicsQueueCreateInfo.flags = 0;
	graphicsQueueCreateInfo.queueFamilyIndex = indices.graphics;
	graphicsQueueCreateInfo.queueCount = 1;
	graphicsQueueCreateInfo.pQueuePriorities = &priority;
	queueCreateInfos.push_back(graphicsQueueCreateInfo);

	bool& hasTransfer = const_cast<bool&>(m_HasTransferQueue);

	if (indices.transfer != indices.graphics) {
		VkDeviceQueueCreateInfo transferQueueCreateInfo;
		transferQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		transferQueueCreateInfo.pNext = nullptr;
		transferQueueCreateInfo.flags = 0;
		transferQueueCreateInfo.queueFamilyIndex = indices.transfer;
		transferQueueCreateInfo.queueCount = 1;
		transferQueueCreateInfo.pQueuePriorities = &priority;
		queueCreateInfos.push_back(transferQueueCreateInfo);
		hasTransfer = true;
	}

	VkDeviceCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.enabledLayerCount = 0;
	createInfo.ppEnabledLayerNames = nullptr;
	createInfo.enabledExtensionCount = (uint32_t)extensionsToRequest.size();
	createInfo.ppEnabledExtensionNames = extensionsToRequest.data();
	createInfo.pEnabledFeatures = &requestedFeatures;

	VkRes(vkCreateDevice(physicalDevice, &createInfo, m_Allocator, &device), "Failed to create logical device");

	std::vector<VkQueue> queues;

	for (size_t i = 0; i < queueCreateInfos.size(); i++) {
		VkQueue queue;
		vkGetDeviceQueue(device, queueCreateInfos[i].queueFamilyIndex, 0, &queue);
		queues.push_back(queue);
	}

	if (!hasTransfer) {
		queues.push_back(queues[0]);
	}

	graphicsQueue = queues[0];
	transferQueue = queues[1];

	graphicsQueueIndex = queueCreateInfos[0].queueFamilyIndex;
	transferQueueIndex = queueCreateInfos[1].queueFamilyIndex;

	return device;
}

QueueFamilyIndex Renderer::FindQueueFamilyIndices(VkPhysicalDevice physicalDevice) const {
	QueueFamilyIndex indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, properties.data());

	std::vector<std::pair<uint32_t, uint32_t>> transferQueues;

	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		bool isGraphics = false;
		bool isCompute = false;
		bool isTransfer = false;
		VkBool32 supportsPresent = false;

		if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			isGraphics = true;
		}
		if (properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
			isTransfer = true;
		}
		if (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			isCompute = true;
		}
	
		vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, i, m_Surface, &supportsPresent);

		if (isGraphics && supportsPresent && indices.graphics == UINT32_MAX) {
			indices.graphics = i;
		}
		if (isTransfer) {
			transferQueues.push_back(std::pair(50, i));
		}
		if (isTransfer && !isCompute) {
			transferQueues.push_back(std::pair(100, i));
		}
		if (!isGraphics && isTransfer && !isCompute) {
			transferQueues.push_back(std::pair(200, i));
		}
	}

	uint32_t lastTransferQueuePoints = 0;
	for (auto& pointsToQueue : transferQueues) {
		if (pointsToQueue.first > lastTransferQueuePoints) {
			indices.transfer = pointsToQueue.second;
			lastTransferQueuePoints = pointsToQueue.first;
		}
	}

	return indices;
}

VkSurfaceFormatKHR Renderer::FindOptimalSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const {
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0);
	std::vector<VkSurfaceFormatKHR> supportedFormats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, supportedFormats.data());

	VkFormat desiredFormat = VK_FORMAT_R8G8B8A8_UNORM;
	VkColorSpaceKHR desiredColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	for (VkSurfaceFormatKHR& format : supportedFormats) {
		if (format.colorSpace == desiredColorSpace && format.format == desiredFormat) {
			return format;
		}
	}
	throw std::exception("Failed to find optimal swapchain format in your GPU!");
}

VkPresentModeKHR Renderer::FindOptimalPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const {
	uint32_t presentModeCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

	bool hasImmediate = false;
	bool hasMailbox = false;
	
	for (VkPresentModeKHR presentMode : presentModes) {
		if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			hasImmediate = true;
		}
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			hasMailbox = true;
		}
	}

	if (hasImmediate) {
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
	} else if (hasMailbox) {
		return VK_PRESENT_MODE_MAILBOX_KHR;
	} else {
		return VK_PRESENT_MODE_FIFO_KHR;
	}
}

VkSwapchainKHR Renderer::CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, std::vector<GPUImage>& images)
{
	VkSwapchainKHR swapchain;
	VkSurfaceFormatKHR format = FindOptimalSwapchainFormat(physicalDevice, surface);
	VkPresentModeKHR presentMode = FindOptimalPresentMode(physicalDevice, surface);

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

	if (surfaceCapabilities.currentExtent.width == 0 && surfaceCapabilities.currentExtent.height == 0) {
		return VK_NULL_HANDLE;
	}

	VkSwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount;
	swapchainCreateInfo.imageFormat = format.format;
	swapchainCreateInfo.imageColorSpace = format.colorSpace;
	swapchainCreateInfo.imageExtent.width = surfaceCapabilities.currentExtent.width;
	swapchainCreateInfo.imageExtent.height = surfaceCapabilities.currentExtent.height;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
	swapchainCreateInfo.queueFamilyIndexCount = m_PhysicalDeviceInfo.queueFamilyCount;
	swapchainCreateInfo.pQueueFamilyIndices = m_PhysicalDeviceInfo.queueFamilyIndices;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if (swapchainCreateInfo.imageExtent.width > surfaceCapabilities.maxImageExtent.width) {
		swapchainCreateInfo.imageExtent.width = surfaceCapabilities.maxImageExtent.width;
	}

	if (swapchainCreateInfo.imageExtent.height > surfaceCapabilities.maxImageExtent.height) {
		swapchainCreateInfo.imageExtent.height = surfaceCapabilities.maxImageExtent.height;
	}


	VkRes(vkCreateSwapchainKHR(device, &swapchainCreateInfo, m_Allocator, &swapchain), "Failed to create swapchain!");

	uint32_t imageCount = 0;
	vkGetSwapchainImagesKHR(m_LogicalDevice, swapchain, &imageCount, nullptr);

	std::vector<VkImage> vulkanImages(imageCount);
	images.resize(imageCount);

	vkGetSwapchainImagesKHR(m_LogicalDevice, swapchain, &imageCount, vulkanImages.data());

	m_Width =  static_cast<uint16_t>(swapchainCreateInfo.imageExtent.width);
	m_Height = static_cast<uint16_t>(swapchainCreateInfo.imageExtent.height);
	
	for (uint32_t i = 0; i < imageCount; i++) {
		images[i].image = vulkanImages[i];
		images[i].format = format;
		images[i].aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		images[i].width = m_Width;
		images[i].height = m_Height;
		CreateImageView(images[i]);
	}

	return swapchain;
}

void Renderer::CreateSynchronizationObjects(uint32_t objectCount, std::vector<VkSemaphore>* imageAcquiredSemaphores, 
											std::vector<VkSemaphore>* imagePresentedSemaphores, std::vector<VkFence>* fences) const {
	if (imageAcquiredSemaphores) imageAcquiredSemaphores->resize(objectCount);
	if (imagePresentedSemaphores) imagePresentedSemaphores->resize(objectCount);
	if (fences) fences->resize(objectCount);

	VkSemaphoreCreateInfo semaphoreCreateInfo;
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = VK_SEMAPHORE_TYPE_BINARY_KHR;

	VkFenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < objectCount; i++) {
		if (imageAcquiredSemaphores) vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, m_Allocator, &(*imageAcquiredSemaphores)[i]);
		if (imagePresentedSemaphores) vkCreateSemaphore(m_LogicalDevice, &semaphoreCreateInfo, m_Allocator, &(*imagePresentedSemaphores)[i]);
		if (fences) vkCreateFence(m_LogicalDevice, &fenceCreateInfo, m_Allocator, &(*fences)[i]);
	}
}

void Renderer::DestroySynchronizationObjects(std::vector<VkSemaphore>* imageAcquiredSemaphores, 
											 std::vector<VkSemaphore>* imagePresentedSemaphores, std::vector<VkFence>* fences) {
	if (imageAcquiredSemaphores) {
		for (VkSemaphore semaphore : *imageAcquiredSemaphores) {
			vkDestroySemaphore(m_LogicalDevice, semaphore, m_Allocator);
		}
	}
	if (imagePresentedSemaphores) {
		for (VkSemaphore semaphore : *imagePresentedSemaphores) {
			vkDestroySemaphore(m_LogicalDevice, semaphore, m_Allocator);
		}
	}
	if (fences) {
		for (VkFence fence : *fences) {
			vkDestroyFence(m_LogicalDevice, fence, m_Allocator);
		}
	}
	if (imageAcquiredSemaphores) imageAcquiredSemaphores->resize(0);
	if (imagePresentedSemaphores) imagePresentedSemaphores->resize(0);
	if (fences) fences->resize(0);
}

void Renderer::CreateImageView(GPUImage& image) const {
	VkImageViewCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.image = image.image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = image.format.format;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = image.aspect;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = image.mipLevels;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	VkRes(vkCreateImageView(m_LogicalDevice, &createInfo, m_Allocator, &image.view), "Failed to create ImageView");
}

void Renderer::CreateSwapchainImageViews(std::vector<GPUImage>& images) {
	std::vector<VkImageView> imageViews(images.size());

	for (GPUImage image : images) {
		CreateImageView(image);
	}
}

void Renderer::DestroyImageView(GPUImage& image) const {
	if (image.view) {
		vkDestroyImageView(m_LogicalDevice, image.view, m_Allocator);
		image.view = 0;
	}
}

void Renderer::DestroyImageViews(const std::vector<GPUImage>& images) const {
	for (const GPUImage& image : images) {
		vkDestroyImageView(m_LogicalDevice, image.view, m_Allocator);
	}
}

VkRenderPass Renderer::CreateRenderPass(VkFormat swapchainFormat, VkFormat depthStencilFormat) const {
	VkRenderPass renderPass;
	
	VkAttachmentDescription colorAttachment;
	colorAttachment.flags = 0;
	colorAttachment.format = swapchainFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment;
	depthAttachment.flags = 0;
	depthAttachment.format = depthStencilFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentReference;
	colorAttachmentReference.attachment = 0;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference;
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription;
	subpassDescription.flags = 0;
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorAttachmentReference;
	subpassDescription.pResolveAttachments = nullptr;
	subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;

	VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };

	VkSubpassDependency subpassDependency;
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	subpassDependency.srcAccessMask = VK_ACCESS_NONE;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependency.dependencyFlags = 0;

	VkRenderPassCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.attachmentCount = _countof(attachments);
	createInfo.pAttachments = attachments;
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpassDescription;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &subpassDependency;

	VkRes(vkCreateRenderPass(m_LogicalDevice, &createInfo, m_Allocator, &renderPass), "Failed to create renderpass!");

	return renderPass;
}

std::vector<VkFramebuffer> Renderer::CreateFramebuffers(const std::vector<GPUImage>& attachments, const GPUImage& depth, VkRenderPass renderPass) const {
	std::vector<VkFramebuffer> framebuffers;

	for (const GPUImage& attachment : attachments) {
		VkImageView attachmentViews[] = { attachment.view, depth.view };
		
		VkFramebufferCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.renderPass = renderPass;
		createInfo.attachmentCount = _countof(attachmentViews);
		createInfo.pAttachments = attachmentViews;
		createInfo.width = attachment.width;
		createInfo.height = attachment.height;
		createInfo.layers = 1;

		VkFramebuffer framebuffer;
		VkRes(vkCreateFramebuffer(m_LogicalDevice, &createInfo, m_Allocator, &framebuffer), "Failed to create framebuffer");
		framebuffers.push_back(framebuffer);
	}

	return framebuffers;
}

void Renderer::DestroyFramebuffers(const std::vector<VkFramebuffer>& framebuffers) {
	for (VkFramebuffer framebuffer : framebuffers) {
		vkDestroyFramebuffer(m_LogicalDevice, framebuffer, m_Allocator);
	}
}

GPUImage Renderer::CreateImage(VkFormat format, VkImageAspectFlags aspect, VkImageUsageFlags usage, 
							   uint16_t width, uint16_t height, uint16_t mipLevels, VkSampleCountFlagBits sampleCount,
							   VkImageTiling tiling) const {
	GPUImage image;
	image.format.format = format;
	image.aspect = aspect;
	image.width = width;
	image.height = height;
	image.mipLevels = mipLevels;
	image.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	VkImageCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.format = format;
	createInfo.extent.width = width;
	createInfo.extent.height = height;
	createInfo.extent.depth = 1;
	createInfo.mipLevels = mipLevels;
	createInfo.arrayLayers = 1;
	createInfo.samples = sampleCount;
	createInfo.tiling = tiling;
	createInfo.usage = usage;
	createInfo.sharingMode = m_PhysicalDeviceInfo.sharingMode;
	createInfo.queueFamilyIndexCount = m_PhysicalDeviceInfo.queueFamilyCount;
	createInfo.pQueueFamilyIndices = m_PhysicalDeviceInfo.queueFamilyIndices;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VkRes(vkCreateImage(m_LogicalDevice, &createInfo, m_Allocator, &image.image), "Failed to create image!");

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(m_LogicalDevice, image.image, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = FindMemoryIndex(memoryRequirements.memoryTypeBits, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT);

	VkRes(vkAllocateMemory(m_LogicalDevice, &allocateInfo, m_Allocator, &image.memory), "Failed to allocate image memory");
	VkRes(vkBindImageMemory(m_LogicalDevice, image.image, image.memory, 0), "Failed to bind image memory");

	return image;
}

uint32_t Renderer::FindMemoryIndex(uint32_t typeFilter, VkMemoryHeapFlags flags) const {
	uint32_t memoryTypeCount = m_PhysicalDeviceInfo.memoryProperties.memoryTypeCount;
	const VkMemoryType* types = m_PhysicalDeviceInfo.memoryProperties.memoryTypes;
	
	for (uint32_t i = 0; i < memoryTypeCount; i++) {
		bool supportedType = typeFilter & (i << 1);
		bool supportedFlag = (types[i].propertyFlags & flags) == flags;

		if (supportedType && supportedFlag) {
			return i;
		}
	}
	return -1;
}

void Renderer::DestroyImage(GPUImage& image) const {
	if (image.memory) {
		vkFreeMemory(m_LogicalDevice, image.memory, m_Allocator);
		image.memory = 0;
	}
	if (image.image) {
		vkDestroyImage(m_LogicalDevice, image.image, m_Allocator);
		image.image = 0;
	}
}

VkFormat Renderer::FindOptimalDepthFormat(VkSurfaceKHR surface) const {
	VkFormatProperties formatProperty;
	vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, VK_FORMAT_D32_SFLOAT, &formatProperty);

	if (formatProperty.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		return VK_FORMAT_D32_SFLOAT;
	}

	vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, VK_FORMAT_D24_UNORM_S8_UINT, &formatProperty);

	if (formatProperty.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		return VK_FORMAT_D24_UNORM_S8_UINT;
	}
	
	throw std::exception("Failed to find an optimal depth format for your GPU!");
}

GPUImage Renderer::CreateDepthBuffer(uint16_t width, uint16_t height, VkSampleCountFlagBits sampleCount) const {
	VkFormat format = FindOptimalDepthFormat(m_Surface);

	GPUImage depth = CreateImage(
		format,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		width, height,
		1,
		sampleCount, 
		VK_IMAGE_TILING_OPTIMAL
	);

	CreateImageView(depth);

	return depth;
}

void Renderer::DestroyDepthBuffer(GPUImage& depthBuffer) const {
	DestroyImageView(depthBuffer);
	DestroyImage(depthBuffer);
}

VkDescriptorPool Renderer::CreateDescriptorPool(VkDescriptorType* descriptorType, uint32_t typesCount, uint32_t numPreAllocatedDescriptors, uint32_t maxDescriptors, bool allowFreeDescriptor) const {
	VkDescriptorPool descriptorPool;

	VkDescriptorPoolSize* sizes = (VkDescriptorPoolSize*)alloca(typesCount * sizeof(VkDescriptorPoolSize));
	for (uint32_t i = 0; i < typesCount; i++)
	{
		sizes[i].type = descriptorType[i];
		sizes[i].descriptorCount = numPreAllocatedDescriptors;	
	}

	VkDescriptorPoolCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = allowFreeDescriptor ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0;
	createInfo.maxSets = maxDescriptors;
	createInfo.poolSizeCount = typesCount;
	createInfo.pPoolSizes = sizes;

	VkRes(vkCreateDescriptorPool(m_LogicalDevice, &createInfo, m_Allocator, &descriptorPool), "Failed to create descriptor pool!");

	return descriptorPool;
}

VkDescriptorSetLayout Renderer::CreateDescriptorSetLayout(VkDescriptorType* descriptorTypes, uint32_t typesCount, uint32_t descriptorCount, VkShaderStageFlagBits shaderStage) const {
	VkDescriptorSetLayout setLayout;

	VkDescriptorSetLayoutBinding* setBinding = (VkDescriptorSetLayoutBinding*)alloca(typesCount * sizeof(VkDescriptorSetLayoutBinding));
	for (uint32_t i = 0; i < typesCount; i++)
	{
		setBinding[i].binding = i;
		setBinding[i].descriptorType = descriptorTypes[i];
		setBinding[i].descriptorCount = descriptorCount;
		setBinding[i].stageFlags = shaderStage;
		setBinding[i].pImmutableSamplers = nullptr;
	}	

	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo;
	setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.pNext = nullptr;
	setLayoutCreateInfo.flags = 0;
	setLayoutCreateInfo.bindingCount = typesCount;
	setLayoutCreateInfo.pBindings = setBinding;

	VkRes(vkCreateDescriptorSetLayout(m_LogicalDevice, &setLayoutCreateInfo, m_Allocator, &setLayout), "Failed to create descriptor set layout");

	return setLayout;
}

VkDescriptorSet Renderer::CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout setLayout, uint32_t setCount) const {
	VkDescriptorSet set;

	VkDescriptorSetAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = setCount;
	allocateInfo.pSetLayouts = &setLayout;

	VkRes(vkAllocateDescriptorSets(m_LogicalDevice, &allocateInfo, &set), "Failed to allocate descriptor set!");

	return set;
}

VkCommandPool Renderer::CreateCommandPool(bool canReset, bool isTransient, uint32_t queueIndex) const {
	VkCommandPool commandPool;

	VkCommandPoolCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	if (canReset) {
		createInfo.flags |= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	}
	if (isTransient) {
		createInfo.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	}
	createInfo.queueFamilyIndex = queueIndex;

	VkRes(vkCreateCommandPool(m_LogicalDevice, &createInfo, m_Allocator, &commandPool), "Failed to create command pool");
	
	return commandPool;
}

VkCommandBuffer Renderer::AllocateCommandBuffer(VkCommandPool commandPool) const {
	VkCommandBuffer commandBuffer;

	VkCommandBufferAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VkRes(vkAllocateCommandBuffers(m_LogicalDevice, &allocateInfo, &commandBuffer), "Failed to allocate command buffer");

	return commandBuffer;
}

void Renderer::CreateGraphicsCommandBuffers(std::vector<VkCommandBuffer>& commandBuffers) const {
	commandBuffers.resize(m_Framecount);
	for (size_t i = 0; i < m_Framecount; i++) {
		commandBuffers[i] = AllocateCommandBuffer(m_GraphicsCommandPool);
	}
}

void Renderer::DestroyGraphicsCommandBuffers(std::vector<VkCommandBuffer>& commandBuffers) const {
	for (VkCommandBuffer cmdBuf : commandBuffers) {
		vkFreeCommandBuffers(m_LogicalDevice, m_GraphicsCommandPool, 1, &cmdBuf);
		cmdBuf = 0;
	}
}

GPUBuffer Renderer::CreateBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryHeapFlags memoryProperties) const {
	GPUBuffer buffer;
	buffer.size = size;

	VkBufferCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.size = size;
	createInfo.usage = usage;
	createInfo.sharingMode = m_PhysicalDeviceInfo.sharingMode;
	createInfo.queueFamilyIndexCount = m_PhysicalDeviceInfo.queueFamilyCount;
	createInfo.pQueueFamilyIndices = m_PhysicalDeviceInfo.queueFamilyIndices;

	VkRes(vkCreateBuffer(m_LogicalDevice, &createInfo, m_Allocator, &buffer.buffer), "Failed to create buffer!");

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(m_LogicalDevice, buffer.buffer, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = FindMemoryIndex(memoryRequirements.memoryTypeBits, memoryProperties);

	VkRes(vkAllocateMemory(m_LogicalDevice, &allocateInfo, m_Allocator, &buffer.memory), "Failed to allocate buffer memory");
	VkRes(vkBindBufferMemory(m_LogicalDevice, buffer.buffer, buffer.memory, 0), "Failed to bind buffer memory");

	return buffer;
}

void Renderer::DestroyBuffer(GPUBuffer& buffer) const {
	vkFreeMemory(m_LogicalDevice, buffer.memory, m_Allocator);
	vkDestroyBuffer(m_LogicalDevice, buffer.buffer, m_Allocator);
	memset(&buffer, 0, sizeof(buffer));
}

Shader Renderer::CreateShader(const char* shaderPath, VkShaderStageFlagBits shaderStage) const {
	Shader shader;
	shader.shaderStage = shaderStage;

	std::ifstream file(shaderPath, std::ios::binary | std::ios::ate);

	if (!file.is_open()) {
		return {};
	}

	size_t size = file.tellg();
	file.seekg(0);
	
	char* spirvCode = (char*)alloca(size);

	file.read(spirvCode, size);
	file.close();

	VkShaderModuleCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.codeSize = size;
	createInfo.pCode = reinterpret_cast<uint32_t*>(spirvCode);

	VkRes(vkCreateShaderModule(m_LogicalDevice, &createInfo, m_Allocator, &shader.shader), "Failed to create shader module");

	return shader;
}

VkPipelineLayout Renderer::CreatePipelineLayout(uint32_t setCount, VkDescriptorSetLayout* setLayout) const {
	VkPipelineLayout layout;

	VkPipelineLayoutCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.setLayoutCount = setCount;
	createInfo.pSetLayouts = setLayout;
	createInfo.pushConstantRangeCount = 0;
	createInfo.pPushConstantRanges = nullptr;

	VkRes(vkCreatePipelineLayout(m_LogicalDevice, &createInfo, m_Allocator, &layout), "Failed to create pipeline layout");

	return layout;
}

VkPipeline Renderer::CreateGraphicsPipeline(Shader* shaders, uint32_t shaderCount, VkPipelineLayout pipelineLayout, VkRenderPass renderPass) const {
	VkPipeline pipeline;

	VkPipelineShaderStageCreateInfo* stageCreateInfos = (VkPipelineShaderStageCreateInfo*)alloca(sizeof(VkPipelineShaderStageCreateInfo) * shaderCount);
	for (uint32_t i = 0; i < shaderCount; i++) {
		Shader& shader = shaders[i];
		stageCreateInfos[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stageCreateInfos[i].pNext = nullptr;
		stageCreateInfos[i].flags = 0;
		stageCreateInfos[i].stage = shader.shaderStage;
		stageCreateInfos[i].module = shader.shader;
		stageCreateInfos[i].pName = shader.entryPoint;
		stageCreateInfos[i].pSpecializationInfo = nullptr;
	}

	VkVertexInputBindingDescription vertexBindings[1];
	vertexBindings[0].binding = 0;
	vertexBindings[0].stride = sizeof(Vertex);
	vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexAttributes[3];
	vertexAttributes[0].location = 0;
	vertexAttributes[0].binding = 0;
	vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributes[0].offset = 0;

	vertexAttributes[1].location = 1;
	vertexAttributes[1].binding = 0;
	vertexAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributes[1].offset = sizeof(glm::vec3);

	vertexAttributes[2].location = 2;
	vertexAttributes[2].binding = 0;
	vertexAttributes[2].format = VK_FORMAT_R32G32_SFLOAT;
	vertexAttributes[2].offset = sizeof(glm::vec3) * 2;

	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo;
	vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputCreateInfo.pNext = nullptr;
	vertexInputCreateInfo.flags = 0;
	vertexInputCreateInfo.vertexBindingDescriptionCount = _countof(vertexBindings);
	vertexInputCreateInfo.pVertexBindingDescriptions = vertexBindings;
	vertexInputCreateInfo.vertexAttributeDescriptionCount = _countof(vertexAttributes);
	vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttributes;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo;
	inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyCreateInfo.pNext = nullptr;
	inputAssemblyCreateInfo.flags = 0;
	inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineTessellationStateCreateInfo tesselationCreateInfo;
	tesselationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tesselationCreateInfo.pNext = nullptr;
	tesselationCreateInfo.flags = 0;
	tesselationCreateInfo.patchControlPoints = 0;

	VkViewport viewport;
	VkRect2D scissor;

	GetViewportAndScissor(viewport, scissor);

	VkPipelineViewportStateCreateInfo viewportCreateInfo;
	viewportCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportCreateInfo.pNext = nullptr;
	viewportCreateInfo.flags = 0;
	viewportCreateInfo.viewportCount = 1;
	viewportCreateInfo.pViewports = &viewport;
	viewportCreateInfo.scissorCount = 1;
	viewportCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo;
	rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationCreateInfo.pNext = nullptr;
	rasterizationCreateInfo.flags = 0;
	rasterizationCreateInfo.depthClampEnable = VK_TRUE;
	rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationCreateInfo.depthBiasClamp = 0.0f;
	rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationCreateInfo.lineWidth = 1.0f;

	VkSampleMask sampleMask = UINT32_MAX;

	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo;
	multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleCreateInfo.pNext = nullptr;
	multisampleCreateInfo.flags = 0;
	multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleCreateInfo.minSampleShading = 0.0f;
	multisampleCreateInfo.pSampleMask = &sampleMask;
	multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo;
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.pNext = nullptr;
	depthStencilCreateInfo.flags = 0;
	depthStencilCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilCreateInfo.front.failOp = VK_STENCIL_OP_ZERO;
	depthStencilCreateInfo.front.passOp = VK_STENCIL_OP_ZERO;
	depthStencilCreateInfo.front.depthFailOp = VK_STENCIL_OP_ZERO;
	depthStencilCreateInfo.front.compareOp = VK_COMPARE_OP_NEVER;
	depthStencilCreateInfo.front.compareMask = 0;
	depthStencilCreateInfo.front.writeMask = 0;
	depthStencilCreateInfo.front.reference = 0;
	depthStencilCreateInfo.back.failOp = VK_STENCIL_OP_ZERO;
	depthStencilCreateInfo.back.passOp = VK_STENCIL_OP_ZERO;
	depthStencilCreateInfo.back.depthFailOp = VK_STENCIL_OP_ZERO;
	depthStencilCreateInfo.back.compareOp = VK_COMPARE_OP_NEVER;
	depthStencilCreateInfo.back.compareMask = 0;
	depthStencilCreateInfo.back.writeMask = 0;
	depthStencilCreateInfo.back.reference = 0;
	depthStencilCreateInfo.minDepthBounds = 0.0f;
	depthStencilCreateInfo.maxDepthBounds = 0.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachments[1];
	colorBlendAttachments[0].blendEnable = VK_FALSE;
	colorBlendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo;
	colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendCreateInfo.pNext = nullptr;
	colorBlendCreateInfo.flags = 0;
	colorBlendCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendCreateInfo.attachmentCount = _countof(colorBlendAttachments);
	colorBlendCreateInfo.pAttachments = colorBlendAttachments;
	memset(colorBlendCreateInfo.blendConstants, 0, sizeof(colorBlendCreateInfo.blendConstants));

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = _countof(dynamicStates);
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.stageCount = shaderCount;
	createInfo.pStages = stageCreateInfos;
	createInfo.pVertexInputState = &vertexInputCreateInfo;
	createInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
	createInfo.pTessellationState = nullptr;
	createInfo.pViewportState = &viewportCreateInfo;
	createInfo.pRasterizationState = &rasterizationCreateInfo;
	createInfo.pMultisampleState = &multisampleCreateInfo;
	createInfo.pDepthStencilState = &depthStencilCreateInfo;
	createInfo.pColorBlendState = &colorBlendCreateInfo;
	createInfo.pDynamicState = &dynamicStateCreateInfo;
	createInfo.layout = pipelineLayout;
	createInfo.renderPass = renderPass;
	createInfo.subpass = 0;
	createInfo.basePipelineHandle = VK_NULL_HANDLE;
	createInfo.basePipelineIndex = 0;

	VkRes(vkCreateGraphicsPipelines(m_LogicalDevice, VK_NULL_HANDLE, 1, &createInfo, m_Allocator, &pipeline), "Failed to create graphics pipeline!");

	return pipeline;
}

void Renderer::GetViewportAndScissor(VkViewport& viewport, VkRect2D& scissor) const {
	viewport.x = 0;
	viewport.y = (float)m_Height;
	viewport.width = (float)m_Width;
	viewport.height = -(float)m_Height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	scissor.extent.width = m_Width;
	scissor.extent.height = m_Height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
}

void Renderer::CreateVertexBuffer() {
	Vertex vBuffer[] = {
		{ {  0.5f,  0.5f, 0.0f } },
		{ {  0.0f, -0.5f, 0.0f } },
		{ { -0.5f,  0.5f, 0.0f } }
	};

	m_VertexBuffer = CreateBuffer(sizeof(vBuffer), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
							VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	GPUBuffer temporaryBuffer = CreateBuffer(sizeof(vBuffer), VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
											 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	
	void* mapped = MapBuffer(temporaryBuffer);
	memcpy(mapped, vBuffer, sizeof(vBuffer));

	VkCommandBuffer commandBuffer = GetTransientTransferCommandBuffer();
	vkWaitForFences(m_LogicalDevice, 1, &m_TransferFences[0], VK_TRUE, UINT64_MAX);
	vkResetFences(m_LogicalDevice, 1, &m_TransferFences[0]);

	VkBufferCopy region;
	region.srcOffset = 0;
	region.dstOffset = 0;
	region.size = sizeof(vBuffer);

	vkCmdCopyBuffer(commandBuffer, temporaryBuffer.buffer, m_VertexBuffer.buffer, 1, &region);
	
	EndTransientTransferCommandBuffer(commandBuffer, m_TransferFences[0]);
	vkWaitForFences(m_LogicalDevice, 1, &m_TransferFences[0], VK_TRUE, UINT64_MAX);
	DestroyBuffer(temporaryBuffer);
}

void Renderer::CreateIndexBuffer() {
	uint32_t indices[] = { 0, 1, 2 };
	m_IndexBuffer = CreateBuffer(sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	GPUBuffer tempBuffer = CreateBuffer(sizeof(indices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
										VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	void* mapped = MapBuffer(tempBuffer);
	memcpy(mapped, indices, sizeof(indices));

	VkCommandBuffer commandBuffer = GetTransientTransferCommandBuffer();
	vkWaitForFences(m_LogicalDevice, 1, &m_TransferFences[0], VK_TRUE, UINT64_MAX);
	vkResetFences(m_LogicalDevice, 1, &m_TransferFences[0]);

	VkBufferCopy region;
	region.srcOffset = 0;
	region.dstOffset = 0;
	region.size = sizeof(indices);

	vkCmdCopyBuffer(commandBuffer, tempBuffer.buffer, m_IndexBuffer.buffer, 1, &region);

	EndTransientTransferCommandBuffer(commandBuffer, m_TransferFences[0]);
	vkWaitForFences(m_LogicalDevice, 1, &m_TransferFences[0], VK_TRUE, UINT64_MAX);
	DestroyBuffer(tempBuffer);
}

VkCommandBuffer Renderer::GetTransientTransferCommandBuffer() const {
	VkCommandBuffer commandBuffer = AllocateCommandBuffer(m_TransferCommandPool);

	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void Renderer::EndTransientTransferCommandBuffer(VkCommandBuffer commandBuffer, VkFence fenceToSignal) const {
	vkEndCommandBuffer(commandBuffer);

	VkPipelineStageFlags stages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };

	VkSubmitInfo submit;
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;
	submit.waitSemaphoreCount = 0;
	submit.pWaitSemaphores = nullptr;
	submit.pWaitDstStageMask = stages;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &commandBuffer;
	submit.signalSemaphoreCount = 0;
	submit.pSignalSemaphores = nullptr;

	vkQueueSubmit(m_TransferQueue, 1, &submit, fenceToSignal);
	if (fenceToSignal) {
		vkWaitForFences(m_LogicalDevice, 1, &fenceToSignal, VK_TRUE, UINT64_MAX);
	} else {
		vkDeviceWaitIdle(m_LogicalDevice);
	}

	vkFreeCommandBuffers(m_LogicalDevice, m_TransferCommandPool, 1, &commandBuffer);
}

void* Renderer::MapBuffer(const GPUBuffer& buffer) const {
	void* mapped;
	vkMapMemory(m_LogicalDevice, buffer.memory, 0, VK_WHOLE_SIZE, 0, &mapped);
	return mapped;
}

void Renderer::UnmapBuffer(const GPUBuffer& buffer) const {
	vkUnmapMemory(m_LogicalDevice, buffer.memory);
}

GPUUniformBuffer Renderer::CreateMVPBuffer() const {
	MVP ubo;
	ubo.model = glm::mat4(1.0f);
	ubo.view = glm::mat4(1.0f);
	ubo.projection = glm::mat4(1.0f);
	ubo.mvp = glm::mat4(1.0f);

	GPUBuffer buffer = CreateBuffer(sizeof(ubo) * m_Framecount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
									VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	
	return buffer;
}

GPUUniformBuffer Renderer::CreateFragmentBuffer() const {
	GPUBuffer buffer = CreateBuffer(sizeof(FragmentBuffer) * m_Framecount, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
									VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	return buffer;
}

void Renderer::UpdateVPBuffer(float deltaTime) {
	// rotating camera 90º to align with the -3.0 offset from the initial camera
	// position which was resulting the camera to start 90º missaligned in the yaw
	float fixedYaw = m_CameraYaw + 90.f;
	float cosYaw = cosf(glm::radians(fixedYaw));
	float sinYaw = sinf(glm::radians(fixedYaw));
	float cosPitch = cosf(glm::radians(m_CameraPitch));

	float radius = 10.f;

#ifdef SHOW_EXTRA_INFO
	Logger::Debug("pitch: %f | yaw: %f\n", m_CameraPitch, m_CameraYaw);
#endif

	glm::vec3 cameraFront;
	cameraFront.x = radius * cosYaw * cosPitch;
	cameraFront.y = radius * sinf(glm::radians(m_CameraPitch));
	cameraFront.z = radius * sinYaw * cosPitch;

	static glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, -3.0f);
	glm::vec3 direction = glm::normalize(cameraFront);
	static glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 rightVector = glm::normalize(glm::cross(up, direction));
	glm::vec3 cameraUp = glm::cross(direction, rightVector);

	float cameraSpeed = deltaTime;

	if (m_Window->IsKeyPressed(KeyCode::Shift)) {
		cameraSpeed *= 4.0f;
	} else {
		cameraSpeed *= 1.0f;
	}
	
	if (!m_MouseShowing)
	{
		if (m_Window->IsKeyPressed(KeyCode::Key_W)) {
			cameraPos += direction * cameraSpeed;
		}
		if (m_Window->IsKeyPressed(KeyCode::Key_S)) {
			cameraPos -= direction * cameraSpeed;
		}
		if (m_Window->IsKeyPressed(KeyCode::Key_D)) {
			cameraPos -= glm::normalize(glm::cross(direction, cameraUp)) * cameraSpeed;
		}
		if (m_Window->IsKeyPressed(KeyCode::Key_A)) {
			cameraPos += glm::normalize(glm::cross(direction, cameraUp)) * cameraSpeed;
		}
		if (m_Window->IsKeyPressed(KeyCode::Space))
		{
			cameraPos += cameraUp * cameraSpeed;
		}
		if (m_Window->IsKeyPressed(KeyCode::Control))
		{
			cameraPos -= cameraUp * cameraSpeed;
		}	
	}

	m_View = glm::lookAtLH(cameraPos, cameraPos + direction, cameraUp);
	m_Projection = glm::perspectiveFovLH(45.f, (float)m_Width, (float)m_Height, 0.1f, 10000.f);
}

void Renderer::UpdateMVPDescriptorSets(const GPUBuffer& buffer, VkDescriptorSet* descriptorSets, uint32_t setCount) const {
	for (uint32_t i = 0; i < setCount; i++) {
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = buffer.buffer;
		bufferInfo.offset = i * sizeof(MVP);
		bufferInfo.range = sizeof(MVP);

		VkWriteDescriptorSet writeSet;
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.pNext = nullptr;
		writeSet.dstSet = descriptorSets[i];
		writeSet.dstBinding = 0;
		writeSet.dstArrayElement = 0;
		writeSet.descriptorCount = 1;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSet.pImageInfo = nullptr;
		writeSet.pBufferInfo = &bufferInfo;
		writeSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_LogicalDevice, 1, &writeSet, 0, nullptr);
	}
}

void Renderer::UpdateFragmentDescriptorSets(const GPUBuffer& buffer, VkDescriptorSet* descriptorSets, uint32_t setCount) const {
	for (uint32_t i = 0; i < setCount; i++) {
		VkDescriptorBufferInfo bufferInfo;
		bufferInfo.buffer = buffer.buffer;
		bufferInfo.offset = i * sizeof(FragmentBuffer);
		bufferInfo.range = sizeof(FragmentBuffer);

		VkWriteDescriptorSet writeSet;
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.pNext = nullptr;
		writeSet.dstSet = descriptorSets[i];
		writeSet.dstBinding = 0;
		writeSet.dstArrayElement = 0;
		writeSet.descriptorCount = 1;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSet.pImageInfo = nullptr;
		writeSet.pBufferInfo = &bufferInfo;
		writeSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_LogicalDevice, 1, &writeSet, 0, nullptr);
	}
}

void Renderer::ImageBarrier(VkCommandBuffer commandBuffer, GPUImage& image, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageLayout oldLayout, VkImageLayout newLayout) const
{
	VkImageSubresourceRange subresource;
	subresource.aspectMask = image.aspect;
	subresource.layerCount = 1;
	subresource.levelCount = 1;
	subresource.baseArrayLayer = 0;
	subresource.baseMipLevel = 0;

	VkImageMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask = srcMask;
	barrier.dstAccessMask = dstMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image.image;
	barrier.subresourceRange = subresource;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr,
		1, &barrier
	);

	image.layout = newLayout;
}

VkSampler Renderer::CreateSampler() const
{
	VkSampler sampler;
	
	VkSamplerCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.mipLodBias = 0.0f;
	createInfo.anisotropyEnable = VK_FALSE;
	createInfo.maxAnisotropy = 1.0f;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_NEVER;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = 1.0f;
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
	createInfo.unnormalizedCoordinates = VK_FALSE;

	VkRes(vkCreateSampler(m_LogicalDevice, &createInfo, m_Allocator, &sampler), "Failed to create sampler!");

	return sampler;
}

VkBool32 Renderer::debugUtilsMessenger(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {
	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);

	if (messageSeverity <= 16 && messageSeverity >= 0) {
		Logger::Info("%s\n", pCallbackData->pMessage);
	} else if (messageSeverity >= 256 && messageSeverity <= 4096) {
		Logger::Error("%s\n", pCallbackData->pMessage);
	}

	return VK_FALSE;
}

void Renderer::RecreateSwapchain() {
	vkDeviceWaitIdle(m_LogicalDevice);
	vkDestroySwapchainKHR(m_LogicalDevice, m_Swapchain, m_Allocator);
	DestroyFramebuffers(m_Framebuffer);
	vkDestroyRenderPass(m_LogicalDevice, m_RenderPass, m_Allocator);
	DestroyImageViews(m_BackBuffers);
	DestroyDepthBuffer(m_DepthBuffer);
	m_Swapchain = VK_NULL_HANDLE;
	m_Framebuffer.resize(0);
	m_BackBuffers.resize(0);
	m_RenderPass = VK_NULL_HANDLE;

	if ((m_Swapchain = CreateSwapchain(m_PhysicalDevice, m_LogicalDevice, m_Surface, m_BackBuffers))) {
		m_DepthBuffer = CreateDepthBuffer(m_Width, m_Height, VK_SAMPLE_COUNT_1_BIT);
		m_RenderPass = CreateRenderPass(m_BackBuffers[0].format.format, m_DepthBuffer.format.format);
		m_Framebuffer = CreateFramebuffers(m_BackBuffers, m_DepthBuffer, m_RenderPass);
	}
}

void Renderer::DrawAllMeshes(VkCommandBuffer commandBuffer, uint32_t frameNum) {
	for (const Scene* scene : m_Meshes) {
		scene->Draw(commandBuffer, frameNum);
	}
}

void Renderer::CalculateAndShowFps(float deltaTime) const {
#ifdef SHOW_EXTRA_INFO
	static float testTimePassed = 0.0f;
	static int frameCount = 0;

	testTimePassed += deltaTime;
	frameCount++;

	if (testTimePassed >= 1.0f) {
		Logger::Debug("FPS: %d\n", frameCount);
		testTimePassed = 0.0f;
		frameCount = 0;
	}
#endif 
}

void Renderer::MouseMoved(int64_t x, int64_t y)
{
	if (m_MouseShowing) return;
	float scalingFactor = 0.5f;
	m_CameraYaw	+= static_cast<float>(-x) * scalingFactor;
	m_CameraPitch += static_cast<float>(-y) * scalingFactor;
	m_CameraPitch = glm::clamp(m_CameraPitch, -89.f, 89.f);
	
}

void Renderer::RegisterForEvents()
{
	EventManager::RegisterListener(EVENT_KEY_PRESSED, this);
	EventManager::RegisterListener(EVENT_KEY_RELEASED, this);
	EventManager::RegisterListener(EVENT_SHOW_CURSOR, this);
	EventManager::RegisterListener(EVENT_HIDE_CURSOR, this);
	EventManager::RegisterListener(EVENT_MOUSE_MOVED, this);
}

