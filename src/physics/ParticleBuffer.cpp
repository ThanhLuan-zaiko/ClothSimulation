#include "ParticleBuffer.h"
#include <glad/glad.h>
#include <iostream>
#include <cstring>

namespace cloth {

ParticleBuffer::ParticleBuffer()
    : m_Buffer(0),
      m_NumParticles(0),
      m_Initialized(false),
      m_MappedPtr(nullptr),
      m_IsPersistentMapped(false) {
}

ParticleBuffer::~ParticleBuffer() {
    if (m_IsPersistentMapped && m_MappedPtr) {
        glUnmapNamedBuffer(m_Buffer);
    }
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

    // PERSISTENT MAPPING: Use glNamedBufferStorage for better performance
    // This avoids CPU-GPU sync overhead on every update
    glNamedBufferStorage(m_Buffer, bufferSize, nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    
    // Map the buffer persistently
    m_MappedPtr = glMapNamedBufferRange(m_Buffer, 0, bufferSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    
    m_IsPersistentMapped = (m_MappedPtr != nullptr);
    
    if (!m_IsPersistentMapped) {
        std::cout << "[ParticleBuffer] Persistent mapping failed, falling back to traditional method" << std::endl;
        // Fallback to traditional method
        glNamedBufferData(m_Buffer, bufferSize, nullptr, GL_DYNAMIC_COPY);
    }

    m_Initialized = true;
}

void ParticleBuffer::UploadData(const std::vector<ParticleData>& particles) {
    if (!m_Initialized || particles.empty()) return;

    size_t count = std::min(particles.size(), m_NumParticles);
    GLsizeiptr bufferSize = sizeof(ParticleData) * count;

    if (m_IsPersistentMapped && m_MappedPtr) {
        // PERSISTENT MAPPING: Direct memcpy to mapped memory
        memcpy(m_MappedPtr, particles.data(), bufferSize);
        glFlushMappedNamedBufferRange(m_Buffer, 0, bufferSize);
    } else {
        // Fallback: Traditional DSA method
        glNamedBufferSubData(m_Buffer, 0, bufferSize, particles.data());
    }
}

void ParticleBuffer::UploadSubset(size_t offset, size_t count, const std::vector<ParticleData>& particles) {
    if (!m_Initialized || particles.empty()) return;

    size_t uploadCount = std::min(particles.size(), count);
    GLsizeiptr bufferSize = sizeof(ParticleData) * uploadCount;
    GLsizeiptr bufferOffset = sizeof(ParticleData) * offset;

    if (m_IsPersistentMapped && m_MappedPtr) {
        // PERSISTENT MAPPING: Direct memcpy to offset in mapped memory
        void* destPtr = static_cast<char*>(m_MappedPtr) + bufferOffset;
        memcpy(destPtr, particles.data(), bufferSize);
        glFlushMappedNamedBufferRange(m_Buffer, bufferOffset, bufferSize);
    } else {
        // Fallback: Traditional DSA method
        glNamedBufferSubData(m_Buffer, bufferOffset, bufferSize, particles.data());
    }
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

    if (m_IsPersistentMapped && m_MappedPtr) {
        // Copy from persistent mapped buffer
        memcpy(particles.data(), m_MappedPtr, sizeof(ParticleData) * m_NumParticles);
    } else {
        // Fallback: Use DSA to get data
        glGetNamedBufferSubData(m_Buffer, 0, sizeof(ParticleData) * m_NumParticles, particles.data());
    }

    return particles;
}

} // namespace cloth
