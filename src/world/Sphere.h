#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace cloth {

// Reflective Sphere object (mirror ball)
class Sphere {
public:
    Sphere(float radius = 1.0f, int segments = 32);
    ~Sphere();

    void Initialize();
    void SetRadius(float radius);
    void SetPosition(const glm::vec3& pos) { m_Position = pos; }
    void SetColor(const glm::vec3& color) { m_Color = color; }

    float GetRadius() const { return m_Radius; }
    glm::vec3 GetPosition() const { return m_Position; }
    glm::vec3 GetColor() const { return m_Color; }

    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetVBO() const { return m_VBO; }
    unsigned int GetEBO() const { return m_EBO; }
    size_t GetIndexCount() const { return m_Indices.size(); }

    void Draw() const;

private:
    void GenerateMesh();
    void SetupBuffers();

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;

    float m_Radius;
    int m_Segments;
    glm::vec3 m_Position;
    glm::vec3 m_Color;

    std::vector<float> m_Vertices;
    std::vector<unsigned int> m_Indices;

    bool m_Initialized;
};

} // namespace cloth
