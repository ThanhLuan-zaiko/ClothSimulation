#include "ClothMesh.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>

namespace cloth {

ClothMesh::ClothMesh(Cloth& cloth)
    : m_Cloth(cloth), m_IsDirty(true), m_VAO(0), m_VBO(0), m_EBO(0) {
    
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    BuildMesh();
    UpdateBuffer();
}

ClothMesh::~ClothMesh() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
}

void ClothMesh::BuildMesh() {
    m_Vertices.clear();
    m_Indices.clear();

    const auto& particles = m_Cloth.GetParticles();
    int width = m_Cloth.GetWidthSegments();
    int height = m_Cloth.GetHeightSegments();

    // Build vertices
    for (int y = 0; y <= height; y++) {
        for (int x = 0; x <= width; x++) {
            int index = y * (width + 1) + x;
            if (index < static_cast<int>(particles.size())) {
                Vertex vertex;
                vertex.position = particles[index]->position;
                vertex.normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default normal
                vertex.texCoord = glm::vec2(
                    static_cast<float>(x) / width,
                    static_cast<float>(y) / height
                );
                m_Vertices.push_back(vertex);
            }
        }
    }

    // Build indices (two triangles per quad)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int topLeft = y * (width + 1) + x;
            int topRight = topLeft + 1;
            int bottomLeft = (y + 1) * (width + 1) + x;
            int bottomRight = bottomLeft + 1;

            // First triangle
            m_Indices.push_back(topLeft);
            m_Indices.push_back(bottomLeft);
            m_Indices.push_back(topRight);

            // Second triangle
            m_Indices.push_back(topRight);
            m_Indices.push_back(bottomLeft);
            m_Indices.push_back(bottomRight);
        }
    }
}

void ClothMesh::Update() {
    // Update vertex positions from particles
    const auto& particles = m_Cloth.GetParticles();

    for (size_t i = 0; i < m_Vertices.size() && i < particles.size(); i++) {
        m_Vertices[i].position = particles[i]->position;
    }

    // Recalculate normals
    for (size_t i = 0; i < m_Indices.size(); i += 3) {
        unsigned int i0 = m_Indices[i];
        unsigned int i1 = m_Indices[i + 1];
        unsigned int i2 = m_Indices[i + 2];

        if (i0 < m_Vertices.size() && i1 < m_Vertices.size() && i2 < m_Vertices.size()) {
            glm::vec3 normal = CalculateNormal(
                m_Vertices[i0].position,
                m_Vertices[i1].position,
                m_Vertices[i2].position
            );
            m_Vertices[i0].normal = normal;
            m_Vertices[i1].normal = normal;
            m_Vertices[i2].normal = normal;
        }
    }

    m_IsDirty = true;
    UpdateBuffer();
}

void ClothMesh::UpdateBuffer() {
    if (!m_IsDirty) return;

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), m_Vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(unsigned int), m_Indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    // TexCoord attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));

    glBindVertexArray(0);
    m_IsDirty = false;
}

void ClothMesh::Draw(Renderer& renderer, const Shader& shader) {
    Update();

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void ClothMesh::DrawWireframe(Renderer& renderer, const Shader& shader) {
    Update();

    glBindVertexArray(m_VAO);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
}

glm::vec3 ClothMesh::CalculateNormal(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3) {
    glm::vec3 edge1 = p2 - p1;
    glm::vec3 edge2 = p3 - p1;
    return glm::normalize(glm::cross(edge1, edge2));
}

} // namespace cloth
