#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

namespace cloth {

/**
 * Interleaved particle data structure for GPU
 * All particle data in a single struct for better cache coherence
 * 
 * Memory layout per particle (48 bytes):
 * - position:  vec4 (xyz = position, w = mass)
 * - prevPos:   vec4 (xyz = previous position, w = padding)
 * - velocity:  vec4 (xyz = velocity, w = pinned: 1.0/0.0)
 */
struct alignas(16) ParticleData {
    glm::vec4 position;      // xyz = position, w = mass
    glm::vec4 prevPosition;  // xyz = previous position, w = padding
    glm::vec4 velocity;      // xyz = velocity, w = pinned (1.0 = pinned, 0.0 = free)
};

static_assert(sizeof(ParticleData) == 48, "ParticleData must be 48 bytes");

/**
 * Manages particle data in a single interleaved GPU buffer (SSBO)
 *
 * Benefits of interleaved buffer:
 * - Single binding point instead of 3
 * - Better cache coherence (related data stored together)
 * - Reduced GPU bind calls
 * - Better memory coalescing for GPU access
 * 
 * DOUBLE BUFFERING SUPPORT:
 * - Uses two buffers for ping-pong rendering
 * - CPU writes to one buffer while GPU reads from the other
 * - Eliminates CPU-GPU synchronization stalls
 */
class ParticleBuffer {
public:
    ParticleBuffer();
    ~ParticleBuffer();

    // Initialize buffer with particle count
    void Initialize(size_t numParticles);
    
    // Initialize double-buffered system (2 buffers for ping-pong)
    void InitializeDoubleBuffered(size_t numParticles);

    // Upload initial particle data to GPU
    void UploadData(const std::vector<ParticleData>& particles);

    // Upload a subset of particles (for efficient updates)
    void UploadSubset(size_t offset, size_t count, const std::vector<ParticleData>& particles);

    // Bind buffer to binding point 0
    void Bind() const;
    
    // Bind specific buffer index (for double buffering)
    void BindBufferIndex(unsigned int index) const;

    // Unbind buffer
    void Unbind() const;

    // Download particle data from GPU (for debugging)
    std::vector<ParticleData> DownloadData() const;

    // Get number of particles
    size_t GetNumParticles() const { return m_NumParticles; }

    // Get buffer ID
    // For double-buffered mode, returns the current READ buffer (what GPU rendered from last frame)
    unsigned int GetBufferID() const { 
        return m_IsDoubleBuffered ? GetBufferID(m_ReadBufferIndex) : m_Buffer; 
    }
    
    // Get buffer ID at index (for double buffering)
    unsigned int GetBufferID(unsigned int index) const;

    // Get buffer size in bytes
    size_t GetBufferSize() const { return m_NumParticles * sizeof(ParticleData); }

    // Persistent mapping support
    void* GetMappedPtr() const { return m_MappedPtr; }
    void* GetMappedPtr(unsigned int index) const;  // For double buffering
    bool IsPersistentMapped() const { return m_IsPersistentMapped; }
    
    // Double buffering control
    void SwapBuffers();  // Swap read/write buffers
    unsigned int GetCurrentReadBuffer() const { return m_ReadBufferIndex; }
    unsigned int GetCurrentWriteBuffer() const { return m_WriteBufferIndex; }
    bool IsDoubleBuffered() const { return m_IsDoubleBuffered; }

private:
    unsigned int m_Buffer;    // Single interleaved SSBO (non-double-buffered mode)
    size_t m_NumParticles;
    bool m_Initialized;

    // Persistent mapping
    void* m_MappedPtr;
    void* m_MappedPtr2;  // Second buffer mapping (for double buffering)
    bool m_IsPersistentMapped;
    
    // Double buffering
    unsigned int m_Buffer2;  // Second buffer for double buffering
    bool m_IsDoubleBuffered;
    unsigned int m_ReadBufferIndex;   // Which buffer GPU is reading
    unsigned int m_WriteBufferIndex;  // Which buffer CPU is writing
};

} // namespace cloth
