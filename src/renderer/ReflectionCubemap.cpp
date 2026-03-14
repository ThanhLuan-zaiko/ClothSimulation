#include "ReflectionCubemap.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace cloth {

ReflectionCubemap::ReflectionCubemap(unsigned int size)
    : m_FBO(0), m_CubemapTexture(0), m_RBO(0), m_Size(size), m_Initialized(false) {

    // Initialize viewport to zeros
    m_Viewport[0] = m_Viewport[1] = m_Viewport[2] = m_Viewport[3] = 0;
    
    // Create cubemap texture
    glGenTextures(1, &m_CubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_CubemapTexture);
    
    for (unsigned int i = 0; i < 6; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, m_Size, m_Size, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Create FBO
    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_CubemapTexture, 0);
    
    // Create renderbuffer for depth
    glGenRenderbuffers(1, &m_RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_Size, m_Size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_RBO);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Reflection cubemap FBO not complete!" << std::endl;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Projection matrix for cubemap (90 degree FOV)
    m_Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    
    m_Initialized = true;
    std::cout << "Reflection cubemap created: " << m_Size << "x" << m_Size << std::endl;
}

ReflectionCubemap::~ReflectionCubemap() {
    if (m_FBO != 0) glDeleteFramebuffers(1, &m_FBO);
    if (m_CubemapTexture != 0) glDeleteTextures(1, &m_CubemapTexture);
    if (m_RBO != 0) glDeleteRenderbuffers(1, &m_RBO);
}

void ReflectionCubemap::BeginRender() {
    // Save current viewport
    glGetIntegerv(GL_VIEWPORT, m_Viewport);

    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Size, m_Size);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ReflectionCubemap::EndRender() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Restore original viewport
    glViewport(m_Viewport[0], m_Viewport[1], m_Viewport[2], m_Viewport[3]);
}

void ReflectionCubemap::BeginFaceRender(unsigned int face) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, m_CubemapTexture, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void ReflectionCubemap::EndFaceRender() {
    // Nothing needed
}

glm::mat4 ReflectionCubemap::GetViewMatrix(unsigned int face, const glm::vec3& position, const glm::vec3& up) {
    glm::vec3 target;
    switch (face) {
        case 0: target = position + glm::vec3(1.0f, 0.0f, 0.0f); break;  // POSITIVE_X
        case 1: target = position + glm::vec3(-1.0f, 0.0f, 0.0f); break; // NEGATIVE_X
        case 2: target = position + glm::vec3(0.0f, 1.0f, 0.0f); break;  // POSITIVE_Y
        case 3: target = position + glm::vec3(0.0f, -1.0f, 0.0f); break; // NEGATIVE_Y
        case 4: target = position + glm::vec3(0.0f, 0.0f, 1.0f); break;  // POSITIVE_Z
        case 5: target = position + glm::vec3(0.0f, 0.0f, -1.0f); break;// NEGATIVE_Z
        default: target = position + glm::vec3(1.0f, 0.0f, 0.0f); break;
    }
    return glm::lookAt(position, target, up);
}

} // namespace cloth
