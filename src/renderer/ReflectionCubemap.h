#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

namespace cloth {

class ReflectionCubemap {
public:
    ReflectionCubemap(unsigned int size = 512);
    ~ReflectionCubemap();

    void BeginRender();
    void EndRender();

    // Render a specific face of the cubemap
    void BeginFaceRender(unsigned int face);
    void EndFaceRender();

    unsigned int GetTextureID() const { return m_CubemapTexture; }
    unsigned int GetFBO() const { return m_FBO; }
    unsigned int GetSize() const { return m_Size; }

    // Get view matrix for each face
    glm::mat4 GetViewMatrix(unsigned int face, const glm::vec3& position, const glm::vec3& up = glm::vec3(0, 1, 0));
    glm::mat4 GetProjectionMatrix() const { return m_Projection; }

private:
    unsigned int m_FBO;
    unsigned int m_CubemapTexture;
    unsigned int m_RBO;
    unsigned int m_Size;
    glm::mat4 m_Projection;
    bool m_Initialized;
    int m_Viewport[4]; // Store original viewport
};

} // namespace cloth
