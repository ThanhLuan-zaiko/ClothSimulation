#include "ClothMesh.h"
#include "physics/GPUPhysicsWorld.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>

namespace cloth {

ClothMesh::ClothMesh(Cloth& cloth)
    : m_Cloth(cloth), m_IsDirty(true), m_VAO(0), m_VBO(0), m_EBO(0), m_GPUVAO(0), m_GPUVAOInitialized(0),
      m_GPUBased(false), m_PosBufferID(0), m_ParticleOffset(0), m_LastKnownBufferID(0) {

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);
    glGenVertexArrays(1, &m_GPUVAO);

    BuildMesh();
    UpdateBuffer();
}

ClothMesh::~ClothMesh() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteVertexArrays(1, &m_GPUVAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
}

void ClothMesh::SetParticleBufferID(unsigned int bufferID) {
    m_PosBufferID = bufferID;
    m_GPUBased = true;
    m_LastKnownBufferID = bufferID;
    SetupGPUVAO();
}

void ClothMesh::SetupGPUVAO() {
    glBindVertexArray(m_GPUVAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    
    // Attribute 0: Position from SoA Buffer (Position is vec4)
    glBindBuffer(GL_ARRAY_BUFFER, m_PosBufferID);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)(m_ParticleOffset * sizeof(glm::vec4)));
    
    // Attribute 1: Normal from VBO (Vertex struct normal)
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    
    // Attribute 2: TexCoord from VBO (Vertex struct texCoord)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    
    glBindVertexArray(0);
}

void ClothMesh::BuildMesh() {
    m_Vertices.clear();
    m_Indices.clear();

    int width = m_Cloth.GetWidthSegments();
    int height = m_Cloth.GetHeightSegments();

    for (int y = 0; y <= height; y++) {
        for (int x = 0; x <= width; x++) {
            Vertex vertex;
            vertex.position = glm::vec3(0.0f);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.texCoord = glm::vec2(static_cast<float>(x) / width, static_cast<float>(y) / height);
            m_Vertices.push_back(vertex);
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int i = y * (width + 1) + x;
            m_Indices.push_back(i); m_Indices.push_back(i + width + 1); m_Indices.push_back(i + 1);
            m_Indices.push_back(i + 1); m_Indices.push_back(i + width + 1); m_Indices.push_back(i + width + 2);
        }
    }
}

void ClothMesh::Update() {
    if (m_GPUBased) return;
    const auto& particles = m_Cloth.GetParticles();
    for (size_t i = 0; i < m_Vertices.size() && i < particles.size(); i++) {
        m_Vertices[i].position = particles[i]->position;
    }
    m_IsDirty = true;
    UpdateBuffer();
}

void ClothMesh::UpdateBuffer() {
    if (!m_IsDirty) return;
    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex), m_Vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(unsigned int), m_Indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
    m_IsDirty = false;
}

void ClothMesh::Draw(const Shader& shader) {
    if (m_GPUBased && m_PosBufferID != 0) {
        glBindVertexArray(m_GPUVAO);
        
        // Ensure the buffer is bound and attributes are set correctly
        glBindBuffer(GL_ARRAY_BUFFER, m_PosBufferID);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)(m_ParticleOffset * sizeof(glm::vec4)));
        
        glDrawElements(GL_TRIANGLES, (GLsizei)m_Indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    } else if (!m_GPUBased) {
        glBindVertexArray(m_VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)m_Indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void ClothMesh::DrawWireframe(const Shader& shader) {
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    Draw(shader);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void ClothMesh::UpdateGPUVAO() {}
void ClothMesh::BindForGPURendering() const {}
void ClothMesh::Unbind() const {}

} // namespace cloth
