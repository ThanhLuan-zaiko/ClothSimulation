#pragma once

#include "Cloth.h"
#include "../renderer/Renderer.h"
#include <vector>
#include <glm/glm.hpp>

namespace cloth {

class ClothMesh {
public:
    ClothMesh(Cloth& cloth);
    ~ClothMesh();

    void Update();
    void Draw(Renderer& renderer, const Shader& shader);
    void DrawWireframe(Renderer& renderer, const Shader& shader);

    int GetVertexCount() const { return static_cast<int>(m_Vertices.size()); }
    int GetIndexCount() const { return static_cast<int>(m_Indices.size()); }

private:
    Cloth& m_Cloth;
    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;
    
    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;
    bool m_IsDirty;

    void BuildMesh();
    void UpdateBuffer();
    glm::vec3 CalculateNormal(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3);
};

} // namespace cloth
