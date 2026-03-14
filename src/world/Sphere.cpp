#include "Sphere.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <iostream>

namespace cloth {

Sphere::Sphere(float radius, int segments)
    : m_Radius(radius), m_Segments(segments), m_Position(0.0f),
      m_Color(1.0f), m_VAO(0), m_VBO(0), m_EBO(0), m_Initialized(false) {
    GenerateMesh();
    SetupBuffers();
    m_Initialized = true;
}

Sphere::~Sphere() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
    }
}

void Sphere::Initialize() {
    if (m_Initialized) return;

    GenerateMesh();
    SetupBuffers();
    m_Initialized = true;
}

void Sphere::SetupBuffers() {
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

void Sphere::GenerateMesh() {
    m_Vertices.clear();
    m_Indices.clear();

    // Sphere using latitude-longitude method
    for (int y = 0; y <= m_Segments; y++) {
        float v = static_cast<float>(y) / static_cast<float>(m_Segments);
        float phi = glm::pi<float>() * v; // 0 to PI

        for (int x = 0; x <= m_Segments; x++) {
            float u = static_cast<float>(x) / static_cast<float>(m_Segments);
            float theta = 2.0f * glm::pi<float>() * u; // 0 to 2*PI

            // Spherical coordinates to Cartesian
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            // Position on unit sphere
            float px = cosTheta * sinPhi;
            float py = cosPhi;
            float pz = sinTheta * sinPhi;

            // Scale by radius
            float vx = px * m_Radius;
            float vy = py * m_Radius;
            float vz = pz * m_Radius;

            // Normal (same as position direction for sphere centered at origin)
            float nx = px;
            float ny = py;
            float nz = pz;

            // Add vertex: position (3) + normal (3)
            m_Vertices.push_back(vx);
            m_Vertices.push_back(vy);
            m_Vertices.push_back(vz);
            m_Vertices.push_back(nx);
            m_Vertices.push_back(ny);
            m_Vertices.push_back(nz);
        }
    }

    // Generate indices for triangle strips
    for (int y = 0; y < m_Segments; y++) {
        for (int x = 0; x < m_Segments; x++) {
            int current = y * (m_Segments + 1) + x;
            int next = current + (m_Segments + 1);

            // Two triangles per quad
            m_Indices.push_back(current);
            m_Indices.push_back(next);
            m_Indices.push_back(current + 1);

            m_Indices.push_back(next);
            m_Indices.push_back(next + 1);
            m_Indices.push_back(current + 1);
        }
    }
}

void Sphere::Draw() const {
    if (!m_Initialized || m_VAO == 0) return;

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void Sphere::SetRadius(float radius) {
    m_Radius = radius;
    GenerateMesh();
    SetupBuffers();
}

} // namespace cloth
