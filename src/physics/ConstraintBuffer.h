#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace cloth {

/**
 * Interleaved constraint data structure for GPU
 * All constraint data in a single struct for better cache coherence
 * 
 * Memory layout per constraint (32 bytes):
 * - indices:  ivec4 (xy = p1,p2 particle indices, z = type, w = padding)
 * - params:   vec2  (x = restLength, y = stiffness)
 * - padding:  vec2  (for 16-byte alignment)
 */
struct alignas(16) ConstraintData {
    glm::ivec4 indices;    // xy = p1, p2 particle indices, z = constraintType, w = padding
    glm::vec2 params;      // x = restLength, y = stiffness
    glm::vec2 padding;     // Alignment to 32 bytes
};

static_assert(sizeof(ConstraintData) == 32, "ConstraintData must be 32 bytes");

/**
 * Manages constraint data in a single interleaved GPU buffer (SSBO)
 * 
 * Benefits of interleaved buffer:
 * - Single binding point instead of 3
 * - Better cache coherence (indices + params stored together)
 * - Reduced GPU bind calls
 * - Better memory coalescing for GPU access
 */
class ConstraintBuffer {
public:
    ConstraintBuffer();
    ~ConstraintBuffer();

    // Initialize buffer with constraint count
    void Initialize(size_t numConstraints);

    // Upload constraint data to GPU
    void UploadData(const std::vector<ConstraintData>& constraints);

    // Upload a subset of constraints (for efficient updates)
    void UploadSubset(size_t offset, size_t count, const std::vector<ConstraintData>& constraints);

    // Bind buffer to binding point 1
    void Bind() const;

    // Unbind buffer
    void Unbind() const;

    // Download constraint data from GPU (for debugging)
    std::vector<ConstraintData> DownloadData() const;

    // Get number of constraints
    size_t GetNumConstraints() const { return m_NumConstraints; }

    // Get buffer ID
    unsigned int GetBufferID() const { return m_Buffer; }

    // Get buffer size in bytes
    size_t GetBufferSize() const { return m_NumConstraints * sizeof(ConstraintData); }

    // Persistent mapping support
    void* GetMappedPtr() const { return m_MappedPtr; }
    bool IsPersistentMapped() const { return m_IsPersistentMapped; }

private:
    unsigned int m_Buffer;    // Single interleaved SSBO
    size_t m_NumConstraints;
    bool m_Initialized;
    
    // Persistent mapping
    void* m_MappedPtr;
    bool m_IsPersistentMapped;
};

} // namespace cloth
