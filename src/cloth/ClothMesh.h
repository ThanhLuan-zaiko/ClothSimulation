#pragma once

#include "Cloth.h"
#include "../renderer/Renderer.h"
#include <vector>
#include <glm/glm.hpp>

namespace cloth {

// Forward declaration to avoid circular dependency
class ParticleBuffer;

class ClothMesh {
public:
    ClothMesh(Cloth& cloth);
    ~ClothMesh();

    // CPU-based update (legacy, for fallback)
    void Update();

    // GPU-based rendering (reads directly from SSBO)
    void Draw(Renderer& renderer, const Shader& shader);
    void DrawWireframe(Renderer& renderer, const Shader& shader);

    // GPU-specific methods
    void SetParticleBuffer(const ParticleBuffer* buffer, size_t offset = 0);
    void BindForGPURendering() const;
    void Unbind() const;

    int GetVertexCount() const { return static_cast<int>(m_Vertices.size()); }
    int GetIndexCount() const { return static_cast<int>(m_Indices.size()); }

    // Check if using GPU rendering
    bool IsGPUBased() const { return m_GPUBased; }
    void SetGPUBased(bool gpuBased) { m_GPUBased = gpuBased; }

private:
    Cloth& m_Cloth;
    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;
    bool m_IsDirty;
    bool m_GPUBased;  // If true, reads from GPU buffer

    // GPU buffer reference
    const ParticleBuffer* m_ParticleBuffer;
    size_t m_ParticleOffset;

    void BuildMesh();
    void UpdateBuffer();
    glm::vec3 CalculateNormal(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3);
};

} // namespace cloth
