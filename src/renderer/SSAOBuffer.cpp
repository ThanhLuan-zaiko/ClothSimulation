#include "SSAOBuffer.h"
#include <iostream>

namespace cloth {

SSAOBuffer::SSAOBuffer(int width, int height) 
    : m_Width(width), m_Height(height), m_FBO(0), m_NormalTexture(0), m_DepthTexture(0), 
      m_SSAOTexture(0), m_BlurredTexture(0) {
    Init();

    // Generate sample kernel
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator) * 2.0 - 1.0,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        
        // Scale samples so they're more clustered near origin
        float scale = (float)i / 64.0f;
        scale = 0.1f + scale * scale * (1.0f - 0.1f);
        sample *= scale;
        m_Samples.push_back(sample);
    }
}

SSAOBuffer::~SSAOBuffer() {
    Cleanup();
}

void SSAOBuffer::Init() {
    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // Normal texture
    glGenTextures(1, &m_NormalTexture);
    glBindTexture(GL_TEXTURE_2D, m_NormalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_Width, m_Height, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_NormalTexture, 0);

    // Depth texture (View-space depth)
    glGenTextures(1, &m_DepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_Width, m_Height, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_DepthTexture, 0);

    // SSAO textures (for compute shader)
    glGenTextures(1, &m_SSAOTexture);
    glBindTexture(GL_TEXTURE_2D, m_SSAOTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_Width, m_Height, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &m_BlurredTexture);
    glBindTexture(GL_TEXTURE_2D, m_BlurredTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, m_Width, m_Height, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Renderbuffer for actual depth test
    GLuint rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_Width, m_Height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "SSAO Framebuffer not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAOBuffer::Cleanup() {
    if (m_FBO) glDeleteFramebuffers(1, &m_FBO);
    if (m_NormalTexture) glDeleteTextures(1, &m_NormalTexture);
    if (m_DepthTexture) glDeleteTextures(1, &m_DepthTexture);
    if (m_SSAOTexture) glDeleteTextures(1, &m_SSAOTexture);
    if (m_BlurredTexture) glDeleteTextures(1, &m_BlurredTexture);
}

void SSAOBuffer::Resize(int width, int height) {
    m_Width = width;
    m_Height = height;
    Cleanup();
    Init();
}

void SSAOBuffer::BindGeometryPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Width, m_Height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void SSAOBuffer::Unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace cloth
