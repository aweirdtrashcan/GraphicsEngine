#pragma once

#include "Mesh.h"

class Node;

class Scene
{
public:
    Scene(const char* path);
    ~Scene();
    void Draw(VkCommandBuffer commandBuffer, uint32_t frameNum) const;
private:
    Node* ParseNode(const struct aiNode* node);
    void ParseMesh(const struct aiScene* scene, void* memory);
private:
    Mesh* m_Meshes;
    uint32_t m_NumMeshes;
    Node* m_RootNode = nullptr;
    glm::mat4 m_Transform = glm::mat4(1.0f);
};

class Node
{
public:
    Node(Mesh** meshes, size_t numMeshes, uint32_t numChildren, const Transform& transform);
    ~Node();
    void Draw(VkCommandBuffer commandBuffer, glm::mat4 accumulatedTransform, uint32_t frameNum);
    void AddChildren(Node* children);
private:
    Transform m_Transform{};
    Mesh** m_Meshes = nullptr;
    Node** m_Children = nullptr;
    size_t m_NumMeshes = 0;
    size_t m_NumChildren = 0;
};