#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <random>

namespace cloth {

class SSAOBuffer {
public:
    SSAOBuffer(int width, int height);
    ~SSAOBuffer();

    void Resize(int width, int height);
    void BindGeometryPass();
    void Unbind();

    GLuint GetNormalTexture() const { return m_NormalTexture; }
    GLuint GetDepthTexture() const { return m_DepthTexture; }
    GLuint GetSSAOTexture() const { return m_SSAOTexture; }
    GLuint GetBlurredTexture() const { return m_BlurredTexture; }
    const std::vector<glm::vec3>& GetSamples() const { return m_Samples; }

private:
    void Init();
    void Cleanup();

    int m_Width, m_Height;
    GLuint m_FBO;
    GLuint m_NormalTexture;
    GLuint m_DepthTexture;
    GLuint m_SSAOTexture;
    GLuint m_BlurredTexture;
    std::vector<glm::vec3> m_Samples;
};

} // namespace cloth
