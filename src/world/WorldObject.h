#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace cloth {

// Base class for world objects (pillars, rocks, etc.)
class WorldObject {
public:
    WorldObject();
    virtual ~WorldObject();

    virtual void Initialize();
    virtual void Draw() const;

    void SetPosition(const glm::vec3& pos) { m_Position = pos; }
    void SetScale(const glm::vec3& scale) { m_Scale = scale; }
    void SetColor(const glm::vec3& color) { m_Color = color; }

    glm::vec3 GetPosition() const { return m_Position; }
    glm::vec3 GetScale() const { return m_Scale; }
    glm::vec3 GetColor() const { return m_Color; }

    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetVBO() const { return m_VBO; }
    unsigned int GetEBO() const { return m_EBO; }
    size_t GetIndexCount() const { return m_Indices.size(); }

protected:
    virtual void GenerateMesh() = 0;
    void SetupBuffers();

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;

    glm::vec3 m_Position;
    glm::vec3 m_Scale;
    glm::vec3 m_Color;

    std::vector<float> m_Vertices;
    std::vector<unsigned int> m_Indices;

    bool m_Initialized;
};

// Pillar/Column object
class Pillar : public WorldObject {
public:
    Pillar(float height = 5.0f, float radius = 0.3f, int segments = 16);

    void SetHeight(float height) { m_Height = height; GenerateMesh(); SetupBuffers(); }
    void SetRadius(float radius) { m_Radius = radius; GenerateMesh(); SetupBuffers(); }

    float GetHeight() const { return m_Height; }
    float GetRadius() const { return m_Radius; }

protected:
    void GenerateMesh() override;

    float m_Height;
    float m_Radius;
    int m_Segments;
};

// Rock object (irregular shape)
class Rock : public WorldObject {
public:
    Rock(float size = 1.0f, float irregularity = 0.3f);

    void SetSize(float size) { m_Size = size; GenerateMesh(); SetupBuffers(); }
    void SetIrregularity(float irr) { m_Irregularity = irr; GenerateMesh(); SetupBuffers(); }

    float GetSize() const { return m_Size; }
    float GetIrregularity() const { return m_Irregularity; }

protected:
    void GenerateMesh() override;

    float m_Size;
    float m_Irregularity;
    int m_Segments;
};

} // namespace cloth
