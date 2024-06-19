#pragma once

#include <vulkan/vulkan.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <exception>
#include <unordered_map>

struct PhysicalDeviceInformation {
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkSharingMode sharingMode;
	uint32_t queueFamilyIndices[2];
	uint32_t queueFamilyCount;
};

struct OptionalVulkanRequest {
	const char* name;
	bool required;
};

struct QueueFamilyIndex {
	uint32_t graphics = UINT32_MAX;
	uint32_t transfer = UINT32_MAX;
};

struct GPUImage {
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkSurfaceFormatKHR format = {};
	uint32_t mipLevels = 1;
	VkImageAspectFlags aspect = {};
	uint16_t width = 0;
	uint16_t height = 0;
};

struct GPUBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint64_t size = 0;
};

struct GPUUniformBuffer : GPUBuffer {
	GPUUniformBuffer(const GPUBuffer& rhs) 
		:
		GPUBuffer(rhs)
	{}
	GPUUniformBuffer() {};
	void* mappedBuffer = nullptr;
};

struct Shader {
	VkShaderModule shader;
	const char* entryPoint = "main";
	VkShaderStageFlagBits shaderStage;
};

struct MVP {
	glm::mat4 model = glm::mat4(1.0f);
	glm::mat4 view = glm::mat4(1.0f);
	glm::mat4 projection = glm::mat4(1.0f);
	glm::mat4 mvp = glm::mat4(1.0f);
};

struct alignas(64) FragmentBuffer {
	glm::vec3 lightPos;
	float constantFalloff;
	glm::vec3 lightColor;
	float linearFalloff;
	float quadraticFalloff;
};

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
};

struct Transform
{
	glm::vec3 pos = { 0.0f, 0.0f, 0.0f };
	glm::vec3 eulerRot = { 0.0f, 0.0f, 0.0f };
	glm::vec3 scale = { 1.0f, 1.0f, 1.0f };

	glm::mat4 model = glm::mat4(1.0f);
};
