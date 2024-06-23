#pragma once

#include "Light.h"
#include "VkStructs.h"
#include "Window.h"
#include "imgui/ImGuiManager.h"

#define VkRes(vkres, message)									\
	{															\
	VkResult __res = (vkres);									\
	if (__res != VK_SUCCESS)									\
		{														\
			Logger::Error("Vulkan Error: %s\n", message);		\
			throw RendererException((message) ? (message) : "");\
		}														\
	}															\

class Window;

enum DescriptorPoolType {
	DESCRIPTOR_POOL_TYPE_UNIFORM_BUFFER,
	DESCRIPTOR_POOL_TYPE_IMGUI,
	DESCRIPTOR_POOL_TYPE_COMBINED_IMAGE_SAMPLER,

	DESCRIPTOR_POOL_TYPE_MAX
};

enum DescriptorSetType
{
	DESCRIPTOR_SET_TYPE_VERTEX_UNIFORM,
	DESCRIPTOR_SET_TYPE_FRAGMENT_UNIFORM,
	DESCRIPTOR_SET_TYPE_FRAGMENT_UNIFORM_BUFFER_COMBINED_IMAGE_SAMPLER,

	DESCRIPTOR_SET_TYPE_MAX
};

enum PipelineType
{
	GRAPHICS_PIPELINE_TYPE_MVP_LIGHT_TEXTURE,
	GRAPHICS_PIPELINE_TYPE_MVP_LIGHT,
	
	GRAPHICS_PIPELINE_TYPE_MAX
};

class Renderer : public IEventListener {
public:
	Renderer(uint16_t width, uint16_t height, Window* window, ImGuiManager& imguiManager);
	virtual ~Renderer() override;
	void OnEvent(EventCode code, const Event& event) override;

public:
	// Public API for (probably) the Engine. 
	bool                        BeginFrame(float deltaTime);
	void						RenderFrame(float deltaTime);
	void                        EndFrame();
	
public:
	// Public API
	VkDevice					GetLogicalDevice() const { return m_LogicalDevice; }
	void*						MapBuffer(const GPUBuffer& buffer) const;
	void						UnmapBuffer(const GPUBuffer& buffer) const;
	void						DestroyBuffer(GPUBuffer& buffer) const;
	void						CreateImageView(GPUImage& image) const;
	void						DestroyImageView(GPUImage& image) const;
	void						DestroyImageViews(const std::vector<GPUImage>& images) const;
	VkCommandBuffer				AllocateCommandBuffer(VkCommandPool commandPool) const;
	void						CreateGraphicsCommandBuffers(std::vector<VkCommandBuffer>& commandBuffers) const;
	void						DestroyGraphicsCommandBuffers(std::vector<VkCommandBuffer>& commandBuffers) const;
	GPUBuffer					CreateBuffer(uint64_t size, VkBufferUsageFlags usage, VkMemoryHeapFlags memoryProperties) const;
	Shader						CreateShader(const char* shaderPath, VkShaderStageFlagBits shaderStage) const;
	GPUImage					CreateImage(VkFormat format, VkImageAspectFlags aspect, VkImageUsageFlags usage, 
											uint16_t width, uint16_t height, uint16_t mipLevels, VkSampleCountFlagBits sampleCount,
											VkImageTiling tiling) const;
	uint32_t					FindMemoryIndex(uint32_t typeFilter, VkMemoryHeapFlags flags) const;
	void						DestroyImage(GPUImage& image) const;
	void						GetViewportAndScissor(VkViewport& viewport, VkRect2D& scissor) const;
	VkPipelineLayout			GetGraphicsPipelineLayout(PipelineType type) const { return m_PipelineLayouts[type]; }
	VkDescriptorSetLayout		GetDescriptorSetLayout(DescriptorSetType type) const { return m_DescriptorSetLayouts[type]; }
	VkDescriptorSet				CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout setLayout, uint32_t setCount) const;
	VkCommandBuffer				GetTransientTransferCommandBuffer(uint8_t threadId, bool isGraphics = false) const;
	void						EndTransientTransferCommandBuffer(VkCommandBuffer commandBuffer, VkFence fenceToSignal, uint8_t threadId, bool isGraphics) const;
	VkDescriptorPool			GetDescriptorPool(uint8_t threadId) const { return m_DescriptorPool[threadId]; }
	void						AddScene(const class Scene* scene);
	VkFence						CreateFence() const;
	void						DestroyFence(VkFence fence) const;
	static const Renderer*		Get();
	const std::vector<Light>	GetWorldLights() const { return m_WorldLights; }
	uint32_t					GetFrameCount() const { return m_Framecount; }
	void						ImageBarrier(VkCommandBuffer commandBuffer, GPUImage& image, VkAccessFlags srcMask, VkAccessFlags dstMask, VkImageLayout oldLayout, VkImageLayout newLayout) const;
	void						ImageBarrier(VkCommandBuffer commandBuffer, GPUImage& image, VkAccessFlags srcMask,
											 VkAccessFlags dstMask, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevel) const;
	VkSampler					GetSampler() const { return m_Sampler; }
	void						CopyBufferToImage(VkCommandBuffer commandBuffer, const GPUBuffer& buffer, const GPUImage& image, VkImageLayout imageLayout) const;
	template<typename T>
	static T					CalculateMipMaps(T width, T height)
	{
		return static_cast<T>(std::floor(std::log2(std::max<T>(width, height)))) + 1;
	}
	void						GenerateMipMaps(VkCommandBuffer commandBuffer, GPUImage& image) const;

private:
	static VkDebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo();

	VkInstance					CreateVulkanInstance() const;
	VkPhysicalDevice			PickPhysicalDevice(VkInstance instance, const VkPhysicalDeviceFeatures& desiredFeatures) const;
	PhysicalDeviceInformation	GetPhysicalDeviceInfo(VkPhysicalDevice physicalDevice) const;
	uint32_t					RankPhysicalDevice(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceFeatures& desiredFeatures) const;
	VkDebugUtilsMessengerEXT	CreateDebugMessenger(VkInstance instance) const;
	void						DestroyDebugUtils(VkInstance instance, VkDebugUtilsMessengerEXT messenger) const;
	VkDevice					CreateLogicalDevice(VkPhysicalDevice physicalDevice, const std::vector<OptionalVulkanRequest>& deviceExtensions,
						                            const VkPhysicalDeviceFeatures& requestedFeatures, VkQueue* graphicsQueue, VkQueue* transferQueue,
						                            uint32_t& graphicsQueueIndex, uint32_t& transferQueueIndex) const;
	QueueFamilyIndex			FindQueueFamilyIndices(VkPhysicalDevice physicalDevice) const;
	VkSwapchainKHR				CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, std::vector<GPUImage>& images);
	VkSurfaceFormatKHR			FindOptimalSwapchainFormat(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const;
	VkPresentModeKHR			FindOptimalPresentMode(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) const;
	void						CreateSynchronizationObjects(uint32_t objectCount, std::vector<VkSemaphore>* imageAcquiredSemaphores, std::vector<VkSemaphore>* imagePresentedSemaphores, std::vector<VkFence>* fences) const;
	void						DestroySynchronizationObjects(std::vector<VkSemaphore>* semaphores, std::vector<VkSemaphore>* imagePresentedSemaphores, std::vector<VkFence>* fences);
	void						CreateSwapchainImageViews(std::vector<GPUImage>& images);
	VkRenderPass				CreateRenderPass(VkFormat swapchainFormat, VkFormat depthStencilFormat) const;
	std::vector<VkFramebuffer>	CreateFramebuffers(const std::vector<GPUImage>& attachments, const GPUImage& depth, VkRenderPass renderPass) const;
	void						DestroyFramebuffers(const std::vector<VkFramebuffer>& framebuffers);
	VkFormat					FindOptimalDepthFormat(VkSurfaceKHR surface) const;
	GPUImage					CreateDepthBuffer(uint16_t width, uint16_t height, VkSampleCountFlagBits sampleCount) const;
	void						DestroyDepthBuffer(GPUImage& depthBuffer) const;
	VkDescriptorPool			CreateDescriptorPool(VkDescriptorType* descriptorType, uint32_t typesCount, uint32_t numPreAllocatedDescriptors = 10, uint32_t maxDescriptors = 100, bool
				                                     allowFreeDescriptor = false) const;
	VkDescriptorSetLayout		CreateDescriptorSetLayout(VkDescriptorType* descriptorTypes, uint32_t typesCount, uint32_t descriptorCount, VkShaderStageFlagBits shaderStage) const;
	VkCommandPool				CreateCommandPool(bool canReset, bool isTransient, uint32_t queueIndex) const;
	VkPipelineLayout			CreatePipelineLayout(uint32_t setCount, VkDescriptorSetLayout* setLayout) const;
	VkPipeline					CreateGraphicsPipeline(Shader* shaders, uint32_t shaderCount, VkPipelineLayout pipelineLayout, VkRenderPass renderPass) const;
	void						CreateVertexBuffer();
	void						CreateIndexBuffer();
	GPUUniformBuffer			CreateMVPBuffer() const;
	GPUUniformBuffer			CreateFragmentBuffer() const;
	void						UpdateVPBuffer(float deltaTime);
	void						UpdateMVPDescriptorSets(const GPUBuffer& buffer, VkDescriptorSet* descriptorSet, uint32_t setCount) const;
	void						UpdateFragmentDescriptorSets(const GPUBuffer& buffer, VkDescriptorSet* descriptorSet, uint32_t setCount) const;
	VkSampler					CreateSampler() const;
	void						SetAttenuationByDistance(size_t lightIndex);
private:
	static VkBool32 VKAPI_PTR debugUtilsMessenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
								 VkDebugUtilsMessageTypeFlagsEXT messageTypes,
								 const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
								 void* pUserData);
	void						RecreateSwapchain();
	void						DrawAllMeshes(VkCommandBuffer commandBuffer, uint32_t frameNum);
	void						CalculateAndShowFps(float deltaTime) const;
	void						MouseMoved(int64_t x, int64_t y);
	void						RegisterForEvents();
	__forceinline ImGui_ImplVulkan_InitInfo	GetVulkanImGuiInitInfo() const;

public:
	static inline constexpr size_t ToMegabyte = 1024 * 1024;
	static inline constexpr size_t ToGigabyte = 1024 * 1024 * 1024;
private:
	VkAllocationCallbacks*			m_Allocator = nullptr;
	VkInstance						m_Instance;
	VkPhysicalDevice				m_PhysicalDevice;
	PhysicalDeviceInformation		m_PhysicalDeviceInfo;
	VkDebugUtilsMessengerEXT		m_DebugMessenger;
	VkDevice						m_LogicalDevice;
	VkSurfaceKHR					m_Surface;
	uint32_t						m_GraphicsQueueIndex;
	uint32_t						m_TransferQueueIndex;
	VkQueue							m_GraphicsQueue[2];
	VkQueue							m_TransferQueue[2];
	VkSwapchainKHR					m_Swapchain;
	std::vector<GPUImage>			m_BackBuffers;
	std::vector<VkSemaphore>		m_GraphicsImageAcquiredSemaphores;
	std::vector<VkSemaphore>		m_GraphicsQueueSubmittedSemaphore;
	std::vector<VkFence>			m_GraphicsFences;
	std::vector<VkFence>			m_TransferFences;
	std::vector<VkFramebuffer>		m_Framebuffer;
	VkRenderPass					m_RenderPass;
	GPUImage						m_DepthBuffer;
	VkDescriptorPool				m_DescriptorPool[2];
	VkDescriptorSetLayout			m_DescriptorSetLayouts[DESCRIPTOR_SET_TYPE_MAX];
	VkCommandPool					m_GraphicsCommandPool[2];
	VkCommandPool					m_TransferCommandPool[2];
	std::vector<VkCommandBuffer>	m_CommandBuffers;
	GPUBuffer						m_VertexBuffer;
	GPUBuffer						m_IndexBuffer;
	VkPipelineLayout				m_PipelineLayouts[GRAPHICS_PIPELINE_TYPE_MAX];
	VkPipeline						m_GraphicsPipelines[GRAPHICS_PIPELINE_TYPE_MAX]{};
	std::vector<Light>				m_WorldLights;
	VkSampler						m_Sampler;
private:
	static inline const Renderer*	s_RendererInstance;
	Window*							m_Window;
	uint32_t						m_Framecount = 0;
	uint16_t						m_Width = 0;
	uint16_t						m_Height = 0;
	uint32_t						m_CurrentFrame = 0;
	bool							m_HasTransferQueue = false;
	ImGuiManager&					m_ImGuiManager;
	uint32_t						m_FrameIndex;
private:
	std::vector<const Scene*>		m_Meshes;
public:
	float							m_CameraPitch;
	float							m_CameraYaw;
	glm::mat4						m_View;
	glm::mat4						m_Projection;
	bool							m_MouseShowing = false;
public:
	// per frame state
	VkCommandBuffer					m_CurrCmdBuf;
	VkFence							m_CurrFence;
	VkSemaphore						m_CurrImgAcq;
	VkSemaphore						m_CurrQueueSubmt;
	VkFramebuffer					m_CurrFb;
};

