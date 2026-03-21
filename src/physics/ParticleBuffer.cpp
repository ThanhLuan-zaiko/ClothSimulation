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
      m_MappedPtr2(nullptr),
      m_IsPersistentMapped(false),
      m_Buffer2(0),
      m_IsDoubleBuffered(false),
      m_ReadBufferIndex(0),
      m_WriteBufferIndex(0) {
}

ParticleBuffer::~ParticleBuffer() {
    // Unmap persistent mappings
    if (m_IsPersistentMapped && m_MappedPtr) {
        glUnmapNamedBuffer(m_Buffer);
        m_MappedPtr = nullptr;
    }
    if (m_IsPersistentMapped && m_MappedPtr2) {
        glUnmapNamedBuffer(m_Buffer2);
        m_MappedPtr2 = nullptr;
    }
    
    // Delete buffers
    if (m_Buffer != 0) {
        glDeleteBuffers(1, &m_Buffer);
    }
    if (m_Buffer2 != 0) {
        glDeleteBuffers(1, &m_Buffer2);
    }
}

void ParticleBuffer::Resize(size_t numParticles) {
    m_NumParticles = numParticles;
    
    if (m_Buffer == 0) {
        // Buffer doesn't exist yet, create it
        glGenBuffers(1, &m_Buffer);
    }
    
    // Calculate buffer size
    GLsizeiptr bufferSize = sizeof(ParticleData) * numParticles;
    
    // Unmap persistent mapping if active
    if (m_IsPersistentMapped && m_MappedPtr) {
        glUnmapNamedBuffer(m_Buffer);
        m_MappedPtr = nullptr;
        m_IsPersistentMapped = false;
    }
    
    // Resize buffer WITHOUT deleting (preserves buffer ID for VAO)
    // Note: This discards old data - caller must re-upload
    glNamedBufferData(m_Buffer, bufferSize, nullptr, GL_DYNAMIC_COPY);
    
    m_Initialized = true;
}

void ParticleBuffer::Initialize(size_t numParticles) {
    m_NumParticles = numParticles;
    m_IsDoubleBuffered = false;  // Single buffer mode

    // Delete old buffers if they exist
    if (m_Buffer != 0) {
        if (m_IsPersistentMapped && m_MappedPtr) {
            glUnmapNamedBuffer(m_Buffer);
            m_MappedPtr = nullptr;
            m_IsPersistentMapped = false;
        }
        if (m_MappedPtr2) {
            glUnmapNamedBuffer(m_Buffer2);
            m_MappedPtr2 = nullptr;
        }
        glDeleteBuffers(1, &m_Buffer);
        m_Buffer = 0;
        if (m_Buffer2 != 0) {
            glDeleteBuffers(1, &m_Buffer2);
            m_Buffer2 = 0;
        }
    }

    // Generate single interleaved buffer
    glGenBuffers(1, &m_Buffer);

    // Calculate buffer size
    GLsizeiptr bufferSize = sizeof(ParticleData) * numParticles;

    // PERSISTENT MAPPING: Use glNamedBufferStorage for better performance
    glNamedBufferStorage(m_Buffer, bufferSize, nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

    // Map the buffer persistently
    m_MappedPtr = glMapNamedBufferRange(m_Buffer, 0, bufferSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

    m_IsPersistentMapped = (m_MappedPtr != nullptr);

    if (!m_IsPersistentMapped) {
        glNamedBufferData(m_Buffer, bufferSize, nullptr, GL_DYNAMIC_COPY);
    }

    m_Initialized = true;
}

void ParticleBuffer::InitializeDoubleBuffered(size_t numParticles) {
    m_NumParticles = numParticles;
    m_IsDoubleBuffered = true;
    m_ReadBufferIndex = 0;
    m_WriteBufferIndex = 1;

    // Delete old buffers if they exist
    if (m_Buffer != 0) {
        if (m_IsPersistentMapped && m_MappedPtr) {
            glUnmapNamedBuffer(m_Buffer);
            m_MappedPtr = nullptr;
        }
        if (m_MappedPtr2) {
            glUnmapNamedBuffer(m_Buffer2);
            m_MappedPtr2 = nullptr;
        }
        glDeleteBuffers(1, &m_Buffer);
        glDeleteBuffers(1, &m_Buffer2);
    }

    // Generate TWO buffers for ping-pong
    glGenBuffers(1, &m_Buffer);
    glGenBuffers(1, &m_Buffer2);

    GLsizeiptr bufferSize = sizeof(ParticleData) * numParticles;

    // Create and map first buffer
    glNamedBufferStorage(m_Buffer, bufferSize, nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    
    m_MappedPtr = glMapNamedBufferRange(m_Buffer, 0, bufferSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

    // Create and map second buffer
    glNamedBufferStorage(m_Buffer2, bufferSize, nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    
    m_MappedPtr2 = glMapNamedBufferRange(m_Buffer2, 0, bufferSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);

    m_IsPersistentMapped = (m_MappedPtr != nullptr && m_MappedPtr2 != nullptr);

    if (!m_IsPersistentMapped) {
        glNamedBufferData(m_Buffer, bufferSize, nullptr, GL_DYNAMIC_COPY);
        glNamedBufferData(m_Buffer2, bufferSize, nullptr, GL_DYNAMIC_COPY);
        m_MappedPtr = nullptr;
        m_MappedPtr2 = nullptr;
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

    if (m_IsDoubleBuffered) {
        unsigned int readBufferID = (m_ReadBufferIndex == 0) ? m_Buffer : m_Buffer2;
        if (readBufferID != 0) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, readBufferID);
        }
    } else {
        if (m_Buffer != 0) {
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_Buffer);
        }
    }
}

void ParticleBuffer::BindBufferIndex(unsigned int index) const {
    if (!m_Initialized) return;
    
    unsigned int bufferID = (index == 0) ? m_Buffer : m_Buffer2;
    if (bufferID != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bufferID);
    }
}

void ParticleBuffer::Unbind() const {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

std::vector<ParticleData> ParticleBuffer::DownloadData() const {
    if (!m_Initialized) return {};

    std::vector<ParticleData> particles(m_NumParticles);

    if (m_IsPersistentMapped && m_MappedPtr) {
        // Copy from current read buffer
        void* srcPtr = (m_IsDoubleBuffered && m_ReadBufferIndex == 1) ? m_MappedPtr2 : m_MappedPtr;
        memcpy(particles.data(), srcPtr, sizeof(ParticleData) * m_NumParticles);
    } else {
        // Fallback: Use DSA to get data
        unsigned int bufferID = (m_IsDoubleBuffered && m_ReadBufferIndex == 1) ? m_Buffer2 : m_Buffer;
        glGetNamedBufferSubData(bufferID, 0, sizeof(ParticleData) * m_NumParticles, particles.data());
    }

    return particles;
}

unsigned int ParticleBuffer::GetBufferID(unsigned int index) const {
    return (index == 0) ? m_Buffer : m_Buffer2;
}

void* ParticleBuffer::GetMappedPtr(unsigned int index) const {
    return (index == 0) ? m_MappedPtr : m_MappedPtr2;
}

void ParticleBuffer::SwapBuffers() {
    if (!m_IsDoubleBuffered) return;
    
    // Swap read and write indices
    // CPU will write to the buffer that GPU just finished reading
    std::swap(m_ReadBufferIndex, m_WriteBufferIndex);
}

} // namespace cloth
