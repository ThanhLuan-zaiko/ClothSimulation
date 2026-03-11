#include "WorldObject.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>
#include <random>

namespace cloth {

WorldObject::WorldObject()
    : m_VAO(0), m_VBO(0), m_EBO(0), m_Position(0.0f), m_Scale(1.0f), 
      m_Color(1.0f), m_Initialized(false) {
}

WorldObject::~WorldObject() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
    }
}

void WorldObject::Initialize() {
    if (m_Initialized) return;
    
    GenerateMesh();
    SetupBuffers();
    m_Initialized = true;
}

void WorldObject::SetupBuffers() {
    if (m_Vertices.empty() || m_Indices.empty()) return;
    
    if (m_VAO == 0) {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_EBO);
    }

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(float),
                 m_Vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(unsigned int),
                 m_Indices.data(), GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);

    // Normal attribute (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}

void WorldObject::Draw() const {
    if (!m_Initialized || m_VAO == 0) return;
    
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ============== Pillar ==============

Pillar::Pillar(float height, float radius, int segments)
    : m_Height(height), m_Radius(radius), m_Segments(segments) {
    m_Color = glm::vec3(0.6f, 0.5f, 0.4f); // Stone color
    Initialize();
}

void Pillar::GenerateMesh() {
    m_Vertices.clear();
    m_Indices.clear();
    
    // Cylinder (pillar) mesh
    float halfHeight = m_Height * 0.5f;
    
    // Top and bottom circles
    for (int i = 0; i <= m_Segments; i++) {
        float angle = (2.0f * glm::pi<float>() * static_cast<float>(i)) / static_cast<float>(m_Segments);
        float x = std::cos(angle) * m_Radius;
        float z = std::sin(angle) * m_Radius;
        
        // Bottom vertex
        m_Vertices.push_back(x);
        m_Vertices.push_back(-halfHeight);
        m_Vertices.push_back(z);
        m_Vertices.push_back(0.0f); // Normal X
        m_Vertices.push_back(-1.0f); // Normal Y
        m_Vertices.push_back(0.0f); // Normal Z
        
        // Top vertex
        m_Vertices.push_back(x);
        m_Vertices.push_back(halfHeight);
        m_Vertices.push_back(z);
        m_Vertices.push_back(0.0f);
        m_Vertices.push_back(1.0f);
        m_Vertices.push_back(0.0f);
    }
    
    // Center vertices for caps
    int centerBottomIdx = static_cast<int>(m_Vertices.size()) / 6;
    m_Vertices.push_back(0.0f);
    m_Vertices.push_back(-halfHeight);
    m_Vertices.push_back(0.0f);
    m_Vertices.push_back(0.0f);
    m_Vertices.push_back(-1.0f);
    m_Vertices.push_back(0.0f);
    
    int centerTopIdx = static_cast<int>(m_Vertices.size()) / 6;
    m_Vertices.push_back(0.0f);
    m_Vertices.push_back(halfHeight);
    m_Vertices.push_back(0.0f);
    m_Vertices.push_back(0.0f);
    m_Vertices.push_back(1.0f);
    m_Vertices.push_back(0.0f);
    
    // Side triangles
    for (int i = 0; i < m_Segments; i++) {
        int bottom1 = i * 2;
        int top1 = i * 2 + 1;
        int bottom2 = (i + 1) * 2;
        int top2 = (i + 1) * 2 + 1;
        
        // Two triangles per segment
        m_Indices.push_back(bottom1);
        m_Indices.push_back(top1);
        m_Indices.push_back(bottom2);
        
        m_Indices.push_back(bottom2);
        m_Indices.push_back(top1);
        m_Indices.push_back(top2);
    }
    
    // Bottom cap
    for (int i = 0; i < m_Segments; i++) {
        m_Indices.push_back(centerBottomIdx);
        m_Indices.push_back((i + 1) * 2);
        m_Indices.push_back(i * 2);
    }
    
    // Top cap
    for (int i = 0; i < m_Segments; i++) {
        m_Indices.push_back(centerTopIdx);
        m_Indices.push_back(i * 2 + 1);
        m_Indices.push_back((i + 1) * 2 + 1);
    }
}

// ============== Rock ==============

Rock::Rock(float size, float irregularity)
    : m_Size(size), m_Irregularity(irregularity), m_Segments(3) {
    m_Color = glm::vec3(0.5f, 0.45f, 0.4f); // Rock color
    Initialize();
}

void Rock::GenerateMesh() {
    m_Vertices.clear();
    m_Indices.clear();
    
    std::random_device rd;
    std::mt19937 gen(42); // Fixed seed for consistency
    std::uniform_real_distribution<float> dis(1.0f - m_Irregularity, 1.0f + m_Irregularity);
    
    // Icosahedron-based sphere
    float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    
    std::vector<glm::vec3> vertices;
    vertices.push_back(glm::normalize(glm::vec3(-1, t, 0)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(1, t, 0)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(-1, -t, 0)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(1, -t, 0)) * m_Size * dis(gen));
    
    vertices.push_back(glm::normalize(glm::vec3(0, -1, t)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(0, 1, t)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(0, -1, -t)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(0, 1, -t)) * m_Size * dis(gen));
    
    vertices.push_back(glm::normalize(glm::vec3(t, 0, -1)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(t, 0, 1)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(-t, 0, -1)) * m_Size * dis(gen));
    vertices.push_back(glm::normalize(glm::vec3(-t, 0, 1)) * m_Size * dis(gen));
    
    // Add vertices with normals
    for (const auto& v : vertices) {
        m_Vertices.push_back(v.x);
        m_Vertices.push_back(v.y);
        m_Vertices.push_back(v.z);
        m_Vertices.push_back(v.x); // Normal (same as position for sphere)
        m_Vertices.push_back(v.y);
        m_Vertices.push_back(v.z);
    }
    
    // Indices for icosahedron faces
    m_Indices = {
        0, 11, 5,  0, 5, 1,  0, 1, 7,  0, 7, 10,  0, 10, 11,
        1, 5, 9,  5, 11, 4,  11, 10, 2,  10, 7, 6,  7, 1, 8,
        3, 9, 4,  3, 4, 2,  3, 2, 6,  3, 6, 8,  3, 8, 9,
        4, 9, 5,  2, 4, 11,  6, 2, 10,  8, 6, 7,  9, 8, 1
    };
}

} // namespace cloth
