#include "ConstraintBuffer.h"
#include <glad/glad.h>
#include <iostream>
#include <cstring>

namespace cloth {

ConstraintBuffer::ConstraintBuffer()
    : m_Buffer(0),
      m_NumConstraints(0),
      m_Initialized(false) {
}

ConstraintBuffer::~ConstraintBuffer() {
    if (m_Buffer != 0) {
        glDeleteBuffers(1, &m_Buffer);
    }
}

void ConstraintBuffer::Initialize(size_t numConstraints) {
    m_NumConstraints = numConstraints;

    // Generate single interleaved buffer
    glGenBuffers(1, &m_Buffer);

    // Calculate buffer size
    GLsizeiptr bufferSize = sizeof(ConstraintData) * numConstraints;

    // Initialize buffer (binding point 1)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, bufferSize, nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    m_Initialized = true;

    std::cout << "[ConstraintBuffer] Initialized " << numConstraints << " constraints ("
              << (bufferSize / 1024 / 1024) << " MB, interleaved)" << std::endl;
}

void ConstraintBuffer::UploadData(const std::vector<ConstraintData>& constraints) {
    if (!m_Initialized || constraints.empty()) return;

    size_t count = std::min(constraints.size(), m_NumConstraints);
    GLsizeiptr bufferSize = sizeof(ConstraintData) * count;

    // Upload all constraint data (binding point 1)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bufferSize, constraints.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[ConstraintBuffer] Uploaded " << count << " constraints to GPU (interleaved)" << std::endl;
}

void ConstraintBuffer::UploadSubset(size_t offset, size_t count, const std::vector<ConstraintData>& constraints) {
    if (!m_Initialized || constraints.empty()) return;

    size_t uploadCount = std::min(constraints.size(), count);
    GLsizeiptr bufferSize = sizeof(ConstraintData) * uploadCount;
    GLsizeiptr bufferOffset = sizeof(ConstraintData) * offset;

    // Upload subset of constraint data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, bufferOffset, bufferSize, constraints.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void ConstraintBuffer::Bind() const {
    if (!m_Initialized) return;

    // Bind interleaved buffer to binding point 1
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_Buffer);
}

void ConstraintBuffer::Unbind() const {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

std::vector<ConstraintData> ConstraintBuffer::DownloadData() const {
    if (!m_Initialized) return {};

    std::vector<ConstraintData> constraints(m_NumConstraints);

    // Map buffer to read data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_Buffer);
    ConstraintData* mappedData = static_cast<ConstraintData*>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));

    if (mappedData) {
        memcpy(constraints.data(), mappedData, sizeof(ConstraintData) * m_NumConstraints);
    }

    // Unmap buffer
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return constraints;
}

} // namespace cloth
