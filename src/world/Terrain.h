#pragma once

#include "renderer/Texture.h"
#include "renderer/Shader.h"
#include "utils/Types.h"
#include <string>
#include <vector>

namespace cloth {

class Terrain {
public:
    Terrain(float width = 20.0f, float depth = 20.0f, int segments = 20);
    ~Terrain();

    void LoadTexture(const std::string& path);
    void SetTexture(const Texture& texture);
    void BindTexture(unsigned int slot = 0) const;
    void UnbindTexture() const;

    void Draw(const Shader& shader) const;

    const Texture& GetTexture() const { return m_Texture; }
    const std::string& GetTexturePath() const { return m_TexturePath; }

    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetIndexCount() const { return static_cast<unsigned int>(m_Indices.size()); }

private:
    void GenerateMesh(float width, float depth, int segments);
    void SetupBuffers();

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;

    Texture m_Texture;
    std::string m_TexturePath;

    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    float m_Width;
    float m_Depth;
    int m_Segments;
};

} // namespace cloth
