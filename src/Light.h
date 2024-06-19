#pragma once
#include <glm/vec3.hpp>

struct Light
{
    Light(glm::vec3 position, glm::vec3 color)
        :
    m_Position(position),
    m_Color(color) {}

    Light() = default;
    
    glm::vec3 m_Position{};
    glm::vec3 m_Color{ 1.0f, 1.0f, 1.0f };
    float constantFalloff = 1.0f;
    float linearFalloff = 0.25f;
    float quadraticFalloff = 0.08f;
};
