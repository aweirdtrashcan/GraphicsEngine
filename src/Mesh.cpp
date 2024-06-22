#include "Mesh.h"

#include "VkStructs.h"
#include "Renderer.h"

#include "Logger.h"
#include "imgui/lib/imgui.h"

#define STB_IMAGE_IMPLEMENTATION
#include "exception/RendererException.h"
#include "stb/stb_image.h"

Mesh::Mesh(Vertex* vertices, uint32_t numVertices, uint32_t* indices, uint32_t numIndices, const char* texturePath, uint8_t threadId)
	:
	m_NumIndices(numIndices),
	m_ThreadId(threadId)
{
	const Renderer* renderer = Renderer::Get();

	size_t vsize = sizeof(Vertex) * numVertices;
	size_t isize = sizeof(uint32_t) * m_NumIndices;

	memcpy(vertices, vertices, vsize);
	memcpy(indices, indices, isize);

	CreateDescriptorSets(renderer);
	CreateUniformBuffers(renderer);
	CreateMeshBuffers(numIndices, indices, numVertices, vertices);
	CreateTextureImage(renderer, texturePath);
	UpdateDescriptorSets(renderer);
}

Mesh::~Mesh() {
	const Renderer* renderer = Renderer::Get();
	vkDeviceWaitIdle(renderer->GetLogicalDevice());
	renderer->DestroyImageView(m_TextureImage);
	renderer->DestroyImage(m_TextureImage);
	delete[] m_VertexDescSet;
	delete[] m_FragmentDescSet;
	renderer->DestroyBuffer(m_VertexUBO);
	renderer->DestroyBuffer(m_FragmentUBO);
	renderer->DestroyBuffer(m_VertexBuffer);
	renderer->DestroyBuffer(m_IndexBuffer);
}

void Mesh::UpdateDescriptorSet(const glm::mat4& transform, uint32_t frameNum) const
{
	const Renderer* renderer = Renderer::Get();

	MVP& gpuMem = ((MVP*)m_VertexUBO.mappedBuffer)[frameNum];
	gpuMem.model = transform;
	gpuMem.view = renderer->m_View;
	gpuMem.projection = renderer->m_Projection;
	gpuMem.mvp = gpuMem.projection * gpuMem.view * gpuMem.model;

	for (const Light& light : Renderer::Get()->GetWorldLights())
	{
		FragmentBuffer* fbgpu = (FragmentBuffer*)m_FragmentUBO.mappedBuffer;
		fbgpu[frameNum].lightPos = light.m_Position;
		fbgpu[frameNum].lightColor = light.m_Color;
		fbgpu[frameNum].constantFalloff = light.constantFalloff;
		fbgpu[frameNum].linearFalloff = light.linearFalloff;
		fbgpu[frameNum].quadraticFalloff = light.quadraticFalloff;
	}
}

void Mesh::Draw(VkCommandBuffer commandBuffer, const glm::mat4& transform, uint32_t frameNum) const {
	const Renderer* renderer = Renderer::Get();

	UpdateDescriptorSet(transform, frameNum);
	
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_VertexBuffer.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	
	VkDescriptorSet sets[] = { m_VertexDescSet[frameNum], m_FragmentDescSet[frameNum] };
	vkCmdBindDescriptorSets(
		commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		renderer->GetGraphicsPipelineLayout(GRAPHICS_PIPELINE_TYPE_MVP_LIGHT_TEXTURE),
		0, _countof(sets),
		sets,
		0, nullptr
	);
	
	vkCmdDrawIndexed(commandBuffer, m_NumIndices, 1, 0, 0, 0);
}

void Mesh::CreateMeshBuffers(uint32_t indexCount, uint32_t* indices, uint32_t vertexCount, Vertex* vertices)
{
	const Renderer* renderer = Renderer::Get();
	GPUBuffer vertexBuffer = renderer->CreateBuffer(
		vertexCount * sizeof(Vertex), 
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	GPUBuffer stagingVertexBuffer = renderer->CreateBuffer(
		indexCount * sizeof(Vertex),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	GPUBuffer indexBuffer = renderer->CreateBuffer(
		m_NumIndices * sizeof(uint32_t),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	GPUBuffer stagingIndexBuffer = renderer->CreateBuffer(
		m_NumIndices * sizeof(uint32_t),
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	
	void* mappedVertexBuffer = renderer->MapBuffer(stagingVertexBuffer);
	memcpy(mappedVertexBuffer, vertices, vertexCount * sizeof(Vertex));

	void* mappedIndexBuffer = renderer->MapBuffer(stagingIndexBuffer);
	memcpy(mappedIndexBuffer, indices, indexCount * sizeof(uint32_t));

	VkCommandBuffer commandBuffer = renderer->GetTransientTransferCommandBuffer(m_ThreadId);

	VkBufferCopy vertexRegion;
	vertexRegion.srcOffset = 0;
	vertexRegion.dstOffset = 0;
	vertexRegion.size = vertexCount * sizeof(Vertex);

	VkBufferCopy indexRegion;
	indexRegion.srcOffset = 0;
	indexRegion.dstOffset = 0;
	indexRegion.size = indexCount * sizeof(uint32_t);

	vkCmdCopyBuffer(commandBuffer, stagingVertexBuffer.buffer, vertexBuffer.buffer, 1, &vertexRegion);
	vkCmdCopyBuffer(commandBuffer, stagingIndexBuffer.buffer, indexBuffer.buffer, 1, &indexRegion);

	VkFence fence = renderer->CreateFence();

	renderer->EndTransientTransferCommandBuffer(commandBuffer, fence, m_ThreadId);

	vkWaitForFences(renderer->GetLogicalDevice(), 1, &fence, VK_TRUE, UINT64_MAX);

	renderer->DestroyFence(fence);

	renderer->DestroyBuffer(stagingIndexBuffer);
	renderer->DestroyBuffer(stagingVertexBuffer);

	m_VertexBuffer = vertexBuffer;
	m_IndexBuffer = indexBuffer;
}

void Mesh::CreateDescriptorSets(const Renderer* renderer)
{
	uint32_t frameNum = renderer->GetFrameCount();
	m_VertexDescSet = new VkDescriptorSet[frameNum];
	m_FragmentDescSet = new VkDescriptorSet[frameNum];
	
	for (uint32_t i = 0; i < frameNum; i++)
	{
		m_VertexDescSet[i] = renderer->CreateDescriptorSet(
			renderer->GetDescriptorPool(m_ThreadId),
			renderer->GetDescriptorSetLayout(DESCRIPTOR_SET_TYPE_VERTEX_UNIFORM), 1
		);

		m_FragmentDescSet[i] = renderer->CreateDescriptorSet(
			renderer->GetDescriptorPool(m_ThreadId),
			renderer->GetDescriptorSetLayout(DESCRIPTOR_SET_TYPE_FRAGMENT_UNIFORM_BUFFER_COMBINED_IMAGE_SAMPLER), 1
		);
	}
}

void Mesh::CreateUniformBuffers(const Renderer* renderer)
{
	m_VertexUBO = renderer->CreateBuffer(
		sizeof(MVP) * renderer->GetFrameCount(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	
	m_VertexUBO.mappedBuffer = renderer->MapBuffer(m_VertexUBO);

	m_FragmentUBO = renderer->CreateBuffer(
		sizeof(FragmentBuffer) * renderer->GetFrameCount(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	m_FragmentUBO.mappedBuffer = renderer->MapBuffer(m_FragmentUBO);

	FragmentBuffer fbcpu{};
	fbcpu.lightPos = { 0.0f, 0.0f, -4.0f };

	FragmentBuffer* fbgpu = (FragmentBuffer*)m_FragmentUBO.mappedBuffer;
	for (uint32_t i = 0; i < renderer->GetFrameCount(); i++)
	{
		fbgpu[i] = fbcpu;
	}
}

void Mesh::UpdateDescriptorSets(const Renderer* renderer)
{
	VkWriteDescriptorSet* writeSets = (VkWriteDescriptorSet*)alloca(
		sizeof(VkWriteDescriptorSet) * renderer->GetFrameCount() * 3
	);

	VkDescriptorBufferInfo* bufferInfos = (VkDescriptorBufferInfo*)alloca(
		sizeof(VkDescriptorBufferInfo) * renderer->GetFrameCount() * 2
	);

	VkDescriptorImageInfo imageInfo;
	imageInfo.sampler = renderer->GetSampler();
	imageInfo.imageView = m_TextureImage.view;
	imageInfo.imageLayout = m_TextureImage.layout;
	
	for (uint32_t i = 0; i < renderer->GetFrameCount(); i++)
	{
		uint32_t writeIndex = i * 3;
		uint32_t bufferIndex = i * 2;
		
		bufferInfos[bufferIndex].buffer = m_VertexUBO.buffer;
		bufferInfos[bufferIndex].offset = i * sizeof(MVP);
		bufferInfos[bufferIndex].range = sizeof(MVP);
		
		writeSets[writeIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSets[writeIndex].pNext = nullptr;
		writeSets[writeIndex].dstSet = m_VertexDescSet[i];
		writeSets[writeIndex].dstBinding = 0;
		writeSets[writeIndex].dstArrayElement = 0;
		writeSets[writeIndex].descriptorCount = 1;
		writeSets[writeIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSets[writeIndex].pImageInfo = nullptr;
		writeSets[writeIndex].pBufferInfo = &bufferInfos[bufferIndex];
		writeSets[writeIndex].pTexelBufferView = nullptr;

		bufferInfos[bufferIndex + 1].buffer = m_FragmentUBO.buffer;
		bufferInfos[bufferIndex + 1].offset = i * sizeof(FragmentBuffer);
		bufferInfos[bufferIndex + 1].range = sizeof(FragmentBuffer);
		
		writeSets[writeIndex + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSets[writeIndex + 1].pNext = nullptr;
		writeSets[writeIndex + 1].dstSet = m_FragmentDescSet[i];
		writeSets[writeIndex + 1].dstBinding = 0;
		writeSets[writeIndex + 1].dstArrayElement = 0;
		writeSets[writeIndex + 1].descriptorCount = 1;
		writeSets[writeIndex + 1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSets[writeIndex + 1].pImageInfo = nullptr;
		writeSets[writeIndex + 1].pBufferInfo = &bufferInfos[bufferIndex + 1];
		writeSets[writeIndex + 1].pTexelBufferView = nullptr;
		
		writeSets[writeIndex + 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSets[writeIndex + 2].pNext = nullptr;
		writeSets[writeIndex + 2].dstSet = m_FragmentDescSet[i];
		writeSets[writeIndex + 2].dstBinding = 1;
		writeSets[writeIndex + 2].dstArrayElement = 0;
		writeSets[writeIndex + 2].descriptorCount = 1;
		writeSets[writeIndex + 2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeSets[writeIndex + 2].pImageInfo = &imageInfo;
		writeSets[writeIndex + 2].pBufferInfo = nullptr;
		writeSets[writeIndex + 2].pTexelBufferView = nullptr;
	}

	vkUpdateDescriptorSets(
		renderer->GetLogicalDevice(),
		renderer->GetFrameCount() * 3,
		writeSets,
		0,
		VK_NULL_HANDLE
	);
}

void Mesh::CreateTextureImage(const Renderer* renderer, const char* texturePath)
{
	int x = 0;
	int y = 0;
	int numChannels = 0;

	if (!texturePath)
	{
		texturePath = "./Models/no_texture.png";
	}
	
	stbi_uc* image = stbi_load(texturePath, &x, &y, &numChannels, 4);

	if (!image)
	{
		Logger::Error("Failed to load image %s\n", texturePath);
		throw RendererException("Failed to load image");
	}

	GPUImage gpuImage = renderer->CreateImage(
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		(uint16_t)x, (int16_t)y,
		1, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL
	);

	renderer->CreateImageView(gpuImage);

	VkCommandBuffer commandBuffer = renderer->GetTransientTransferCommandBuffer(m_ThreadId);
	
	renderer->ImageBarrier(
		commandBuffer,
		gpuImage,
		VK_ACCESS_NONE,
		VK_ACCESS_MEMORY_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);
	
	uint64_t imageSizeInBytes = (uint64_t)x * (uint64_t)y * (uint64_t)4;

	VkFence fence = renderer->CreateFence();
	
	GPUUniformBuffer stagingBuffer = renderer->CreateBuffer(
		imageSizeInBytes,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	stagingBuffer.mappedBuffer = renderer->MapBuffer(stagingBuffer);
	
	memcpy(stagingBuffer.mappedBuffer, image, imageSizeInBytes);
	
	renderer->CopyBufferToImage(commandBuffer, stagingBuffer, gpuImage);
	
	renderer->ImageBarrier(
		commandBuffer,
		gpuImage,
		VK_ACCESS_MEMORY_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	);
	
	renderer->EndTransientTransferCommandBuffer(commandBuffer, fence, m_ThreadId);

	vkWaitForFences(renderer->GetLogicalDevice(), 1, &fence, VK_TRUE, UINT64_MAX);

	renderer->DestroyFence(fence);
	renderer->DestroyBuffer(stagingBuffer);

	m_TextureImage = gpuImage;
}
