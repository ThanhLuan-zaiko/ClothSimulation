#include "ConstraintBuffer.h"
#include <glad/glad.h>
#include <iostream>
#include <cstring>

namespace cloth {

ConstraintBuffer::ConstraintBuffer()
    : m_Buffer(0),
      m_NumConstraints(0),
      m_Initialized(false),
      m_MappedPtr(nullptr),
      m_IsPersistentMapped(false) {
}

ConstraintBuffer::~ConstraintBuffer() {
    if (m_IsPersistentMapped && m_MappedPtr) {
        glUnmapNamedBuffer(m_Buffer);
    }
    if (m_Buffer != 0) {
        glDeleteBuffers(1, &m_Buffer);
    }
}

void ConstraintBuffer::Initialize(size_t numConstraints) {
    m_NumConstraints = numConstraints;

    // Delete old buffer if it exists (prevent leak when re-initializing)
    if (m_Buffer != 0) {
        if (m_IsPersistentMapped && m_MappedPtr) {
            glUnmapNamedBuffer(m_Buffer);
            m_MappedPtr = nullptr;
            m_IsPersistentMapped = false;
        }
        glDeleteBuffers(1, &m_Buffer);
        m_Buffer = 0;
    }

    // Generate single interleaved buffer
    glGenBuffers(1, &m_Buffer);

    // Calculate buffer size
    GLsizeiptr bufferSize = sizeof(ConstraintData) * numConstraints;

    // PERSISTENT MAPPING: Use glNamedBufferStorage for better performance
    glNamedBufferStorage(m_Buffer, bufferSize, nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    
    // Map the buffer persistently
    m_MappedPtr = glMapNamedBufferRange(m_Buffer, 0, bufferSize,
        GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    
    m_IsPersistentMapped = (m_MappedPtr != nullptr);
    
    if (!m_IsPersistentMapped) {
        std::cout << "[ConstraintBuffer] Persistent mapping failed, falling back to traditional method" << std::endl;
        glNamedBufferData(m_Buffer, bufferSize, nullptr, GL_DYNAMIC_COPY);
    }

    m_Initialized = true;
}

void ConstraintBuffer::UploadData(const std::vector<ConstraintData>& constraints) {
    if (!m_Initialized || constraints.empty()) return;

    size_t count = std::min(constraints.size(), m_NumConstraints);
    GLsizeiptr bufferSize = sizeof(ConstraintData) * count;

    if (m_IsPersistentMapped && m_MappedPtr) {
        // PERSISTENT MAPPING: Direct memcpy to mapped memory
        memcpy(m_MappedPtr, constraints.data(), bufferSize);
        glFlushMappedNamedBufferRange(m_Buffer, 0, bufferSize);
    } else {
        // Fallback: Traditional DSA method
        glNamedBufferSubData(m_Buffer, 0, bufferSize, constraints.data());
    }
}

void ConstraintBuffer::UploadSubset(size_t offset, size_t count, const std::vector<ConstraintData>& constraints) {
    if (!m_Initialized || constraints.empty()) return;

    size_t uploadCount = std::min(constraints.size(), count);
    GLsizeiptr bufferSize = sizeof(ConstraintData) * uploadCount;
    GLsizeiptr bufferOffset = sizeof(ConstraintData) * offset;

    if (m_IsPersistentMapped && m_MappedPtr) {
        // PERSISTENT MAPPING: Direct memcpy to offset in mapped memory
        void* destPtr = static_cast<char*>(m_MappedPtr) + bufferOffset;
        memcpy(destPtr, constraints.data(), bufferSize);
        glFlushMappedNamedBufferRange(m_Buffer, bufferOffset, bufferSize);
    } else {
        // Fallback: Traditional DSA method
        glNamedBufferSubData(m_Buffer, bufferOffset, bufferSize, constraints.data());
    }
}

void ConstraintBuffer::Bind() const {
    if (!m_Initialized || m_Buffer == 0) {
        return;  // Don't bind invalid buffers
    }

    // Bind interleaved buffer to binding point 1
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_Buffer);
}

void ConstraintBuffer::Unbind() const {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

std::vector<ConstraintData> ConstraintBuffer::DownloadData() const {
    if (!m_Initialized) return {};

    std::vector<ConstraintData> constraints(m_NumConstraints);

    if (m_IsPersistentMapped && m_MappedPtr) {
        // Copy from persistent mapped buffer
        memcpy(constraints.data(), m_MappedPtr, sizeof(ConstraintData) * m_NumConstraints);
    } else {
        // Fallback: Use DSA to get data
        glGetNamedBufferSubData(m_Buffer, 0, sizeof(ConstraintData) * m_NumConstraints, constraints.data());
    }

    return constraints;
}

} // namespace cloth
