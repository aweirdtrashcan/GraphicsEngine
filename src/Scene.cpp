#include "Scene.h"

#include "Engine.h"
#include "Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "exception/RendererException.h"

Scene::Scene(const char* path)
{
    Assimp::Importer imp;
    const aiScene* scene = imp.ReadFile(path, aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
        aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);

    if (!scene)
    {
        Logger::Error("Failed to import model: %s: %s\n", path, imp.GetErrorString());
        throw RendererException("Failed to load a model. Check console for extra info.");
    }

    m_Meshes = (Mesh*)malloc(sizeof(Mesh) * scene->mNumMeshes);
    m_NumMeshes = scene->mNumMeshes;
    
    ParseMesh(scene, m_Meshes);
    m_RootNode = ParseNode(scene->mRootNode);
}

Scene::~Scene()
{
    delete m_RootNode;
    for (uint32_t i = 0; i < m_NumMeshes; i++)
    {
        m_Meshes[i].~Mesh();
    }
    free(m_Meshes);
}

void Scene::Draw(VkCommandBuffer commandBuffer, uint32_t frameNum) const
{
    m_RootNode->Draw(commandBuffer, m_Transform, frameNum);
}

Node* Scene::ParseNode(const aiNode* node)
{
    Transform nodeTransform;
    nodeTransform.model = *(glm::mat4*)&node->mTransformation;

    Mesh** meshes = new Mesh*[node->mNumMeshes];

    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        uint32_t meshId = node->mMeshes[i];
        meshes[i] = &m_Meshes[meshId];
    }

    Node* newNode = new Node(meshes, node->mNumMeshes, node->mNumChildren, nodeTransform);
    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        newNode->AddChildren(ParseNode(node->mChildren[i]));
    }

    return newNode;
}

void Scene::ParseMesh(const aiScene* scene, void* memory)
{
    for (uint32_t i = 0; i < scene->mNumMeshes; i++)
    {
        const aiMesh* mesh = scene->mMeshes[i];
        const uint32_t vertCount = mesh->mNumVertices;
        const uint32_t faceCount = mesh->mNumFaces * 3;
        
        Vertex* vertices = new Vertex[vertCount];
        uint32_t* indices = new uint32_t[faceCount];
        
        for (uint32_t j = 0; j < vertCount; j++)
        {
            vertices[j].pos = *reinterpret_cast<glm::vec3*>(&mesh->mVertices[j]);
            if (mesh->mNormals)
            {
                vertices[j].normal = *reinterpret_cast<glm::vec3*>(&mesh->mNormals[j]);
            }
        }

        uint32_t currentIndex = 0;
        for (uint32_t k = 0; k < mesh->mNumFaces; k++)
        {
            assert(mesh->mFaces[k].mNumIndices == 3 && "Expected 3 indices per face.");
            indices[currentIndex++] = mesh->mFaces[k].mIndices[0];
            indices[currentIndex++] = mesh->mFaces[k].mIndices[1];
            indices[currentIndex++] = mesh->mFaces[k].mIndices[2];
        }

        new (&static_cast<Mesh*>(memory)[i]) Mesh(vertices, vertCount, indices, faceCount);

        delete[] vertices;
        delete[] indices;
    }
}

Node::Node(Mesh** meshes, size_t numMeshes, uint32_t numChildren, const Transform& transform)
    :
    m_Transform(transform),
    m_Meshes(meshes),
    m_NumMeshes(numMeshes)
{
    m_Children = (Node**)malloc(sizeof(Node*) * numChildren);
}

Node::~Node()
{
    delete[] m_Meshes;
    m_NumMeshes = 0;
    for (uint32_t i = 0; i < m_NumChildren; i++)
    {
        m_Children[i]->~Node();
    }
    free(m_Children);
    m_Children = nullptr;
    m_NumChildren = 0;
}

void Node::Draw(VkCommandBuffer commandBuffer, glm::mat4 accumulatedTransform, uint32_t frameNum)
{
    glm::mat4 mat = m_Transform.model * accumulatedTransform;
    
    for (size_t i = 0; i < m_NumMeshes; i++)
        m_Meshes[i]->Draw(commandBuffer, mat, frameNum);
    for (size_t i = 0; i < m_NumChildren; i++)
        m_Children[i]->Draw(commandBuffer, mat, frameNum);
}

void Node::AddChildren(Node* children)
{
    m_Children[m_NumChildren] = children;
    m_NumChildren++;
}
