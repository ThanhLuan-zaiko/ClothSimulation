#include "ParticleBuffer.h"
#include <glad/glad.h>
#include <iostream>
#include <cstring>

namespace cloth {

ParticleBuffer::ParticleBuffer()
    : m_Buffer(0),
      m_NumParticles(0),
      m_Initialized(false) {
}

ParticleBuffer::~ParticleBuffer() {
    if (m_Buffer != 0) {
        glDeleteBuffers(1, &m_Buffer);
    }
}

void ParticleBuffer::Initialize(size_t numParticles) {
    m_NumParticles = numParticles;

    // Generate single interleaved buffer
    glGenBuffers(1, &m_Buffer);

    // Calculate buffer size
    GLsizeiptr bufferSize = sizeof(ParticleData) * numParticles;

    // Initialize buffer (binding point 0)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_Initialized = true;

    std::cout << "[ParticleBuffer] Initialized " << numParticles << " particles ("
              << (bufferSize / 1024 / 1024) << " MB, interleaved)" << std::endl;
}

void ParticleBuffer::UploadData(const std::vector<ParticleData>& particles) {
    if (!m_Initialized || particles.empty()) return;

    size_t count = std::min(particles.size(), m_NumParticles);
    GLsizeiptr bufferSize = sizeof(ParticleData) * count;

    // Upload all particle data (binding point 0)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bufferSize, particles.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[ParticleBuffer] Uploaded " << count << " particles to GPU (interleaved)" << std::endl;
}

void ParticleBuffer::UploadSubset(size_t offset, size_t count, const std::vector<ParticleData>& particles) {
    if (!m_Initialized || particles.empty()) return;

    size_t uploadCount = std::min(particles.size(), count);
    GLsizeiptr bufferSize = sizeof(ParticleData) * uploadCount;
    GLsizeiptr bufferOffset = sizeof(ParticleData) * offset;

    // Upload subset of particle data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, bufferOffset, bufferSize, particles.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ParticleBuffer::Bind() const {
    if (!m_Initialized) return;

    // Bind interleaved buffer to binding point 0
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_Buffer);
}

void ParticleBuffer::Unbind() const {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

std::vector<ParticleData> ParticleBuffer::DownloadData() const {
    if (!m_Initialized) return {};

    std::vector<ParticleData> particles(m_NumParticles);

    // Map buffer to read data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    ParticleData* mappedData = static_cast<ParticleData*>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));

    if (mappedData) {
        memcpy(particles.data(), mappedData, sizeof(ParticleData) * m_NumParticles);
    }

    // Unmap buffer
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return particles;
}

} // namespace cloth
