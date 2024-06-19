#include "Mesh.h"

#include "VkStructs.h"
#include "Renderer.h"

#include "Logger.h"
#include "imgui/lib/imgui.h"

Mesh::Mesh(Vertex* vertices, uint32_t numVertices, uint32_t* indices, uint32_t numIndices)
	:
	m_NumIndices(numIndices)
{
	const Renderer* renderer = Renderer::Get();

	size_t vsize = sizeof(Vertex) * numVertices;
	size_t isize = sizeof(uint32_t) * m_NumIndices;

	memcpy(vertices, vertices, vsize);
	memcpy(indices, indices, isize);

	CreateDescriptorSets(renderer);
	CreateUniformBuffers(renderer);
	CreateMeshBuffers(numIndices, indices, numVertices, vertices);
	UpdateDescriptorSets(renderer);
}

Mesh::~Mesh() {
	const Renderer* renderer = Renderer::Get();
	vkDeviceWaitIdle(renderer->GetLogicalDevice());
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
		renderer->GetGraphicsPipelineLayout(),
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

	VkCommandBuffer commandBuffer = renderer->GetTransientTransferCommandBuffer();

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

	renderer->EndTransientTransferCommandBuffer(commandBuffer, VK_NULL_HANDLE);

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
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			renderer->GetDescriptorPool(DESCRIPTOR_POOL_TYPE_UNIFORM_BUFFER),
			renderer->GetVertexMVPSetLayout()
		);

		m_FragmentDescSet[i] = renderer->CreateDescriptorSet(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			renderer->GetDescriptorPool(DESCRIPTOR_POOL_TYPE_UNIFORM_BUFFER),
			renderer->GetFragmentLightSetLayout()
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
		sizeof(VkWriteDescriptorSet) * renderer->GetFrameCount() * 2
	);

	VkDescriptorBufferInfo* bufferInfos = (VkDescriptorBufferInfo*)alloca(
		sizeof(VkDescriptorBufferInfo) * renderer->GetFrameCount() * 2
	);
	
	for (uint32_t i = 0; i < renderer->GetFrameCount(); i++)
	{
		uint32_t index = i * 2;
		
		bufferInfos[index].buffer = m_VertexUBO.buffer;
		bufferInfos[index].offset = i * sizeof(MVP);
		bufferInfos[index].range = sizeof(MVP);
		
		writeSets[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSets[index].pNext = nullptr;
		writeSets[index].dstSet = m_VertexDescSet[i];
		writeSets[index].dstBinding = 0;
		writeSets[index].dstArrayElement = 0;
		writeSets[index].descriptorCount = 1;
		writeSets[index].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSets[index].pImageInfo = nullptr;
		writeSets[index].pBufferInfo = &bufferInfos[index];
		writeSets[index].pTexelBufferView = nullptr;

		bufferInfos[index + 1].buffer = m_FragmentUBO.buffer;
		bufferInfos[index + 1].offset = i * sizeof(FragmentBuffer);
		bufferInfos[index + 1].range = sizeof(FragmentBuffer);
		
		writeSets[index + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSets[index + 1].pNext = nullptr;
		writeSets[index + 1].dstSet = m_FragmentDescSet[i];
		writeSets[index + 1].dstBinding = 0;
		writeSets[index + 1].dstArrayElement = 0;
		writeSets[index + 1].descriptorCount = 1;
		writeSets[index + 1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSets[index + 1].pImageInfo = nullptr;
		writeSets[index + 1].pBufferInfo = &bufferInfos[index + 1];
		writeSets[index + 1].pTexelBufferView = nullptr;
	}

	vkUpdateDescriptorSets(
		renderer->GetLogicalDevice(),
		renderer->GetFrameCount() * 2,
		writeSets,
		0,
		VK_NULL_HANDLE
	);
}
