#pragma once

#include "Engine.h"
#include "VkStructs.h"

struct Vertex;
class Renderer;
struct Light;

class Mesh {
public:
	Mesh(Vertex* vertices, uint32_t numVertices, uint32_t* indices, uint32_t numIndices);
	Mesh(const Mesh& rhs) = delete;
	Mesh& operator=(const Mesh& rhs) = delete;
	~Mesh();
	void UpdateDescriptorSet(const glm::mat4& transform, uint32_t frameNum) const;

	void Draw(VkCommandBuffer commandBuffer, const glm::mat4& transform, uint32_t frameNum) const;
private:
	void CreateMeshBuffers(uint32_t indexCount, uint32_t* indices, uint32_t vertexCount, Vertex* vertices);
	void CreateDescriptorSets(const Renderer* renderer);
	void CreateUniformBuffers(const Renderer* renderer);
	void UpdateDescriptorSets(const Renderer* renderer);
	
protected:
	static constexpr uint64_t offsets[1] = {};

	uint32_t m_NumIndices;
	
	GPUBuffer m_VertexBuffer;
	GPUBuffer m_IndexBuffer;

	VkDescriptorSet* m_VertexDescSet;
	VkDescriptorSet* m_FragmentDescSet;
	
	GPUUniformBuffer m_VertexUBO;
	GPUUniformBuffer m_FragmentUBO;
};

