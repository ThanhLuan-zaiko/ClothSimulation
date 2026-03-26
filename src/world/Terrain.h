#pragma once

#include "renderer/Texture.h"
#include "renderer/Shader.h"
#include "utils/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace cloth {

class Terrain {
public:
    Terrain(float width = 50.0f, float depth = 50.0f, int segments = 50, float textureTiling = 5.0f);
    ~Terrain();

    // Deferred initialization - call after GL context is ready
    void InitializeMesh();
    bool IsMeshInitialized() const { return m_MeshInitialized; }

    void LoadTexture(const std::string& path);
    void SetTexture(Texture&& texture);
    void BindTexture(unsigned int slot = 0) const;
    void UnbindTexture() const;

    void Draw(const Shader& shader) const;

    const Texture& GetTexture() const { return m_Texture; }
    const std::string& GetTexturePath() const { return m_TexturePath; }

    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetIndexCount() const { return static_cast<unsigned int>(m_Indices.size()); }

    float GetTextureTiling() const { return m_TextureTiling; }
    void SetTextureTiling(float tiling) { m_TextureTiling = tiling; }

    float GetWidth() const { return m_Width; }
    float GetDepth() const { return m_Depth; }
    int GetSegments() const { return m_Segments; }
    glm::vec2 GetSize() const { return glm::vec2(m_Width, m_Depth); }

    std::vector<float> GetHeightData() const {
        std::vector<float> data;
        data.reserve(m_Vertices.size());
        for (const auto& v : m_Vertices) {
            data.push_back(v.position.y);
        }
        return data;
    }

    // Heightmap controls
    void SetHeightScale(float scale) { m_HeightScale = scale; RegenerateMesh(); }
    float GetHeightScale() const { return m_HeightScale; }

    void SetWireframe(bool wireframe) { m_Wireframe = wireframe; }
    bool IsWireframe() const { return m_Wireframe; }

    // Generate height using different methods
    void GenerateFlatHeightmap();
    void GenerateNoiseHeightmap(float amplitude = 2.0f, float frequency = 0.5f);
    void GenerateMountainHeightmap(float peakHeight = 5.0f);
    void GenerateValleyHeightmap(float depth = 3.0f);
    
    // Get terrain height at specific (x, z) position
    float GetHeightAt(float x, float z) const;

    // Get terrain normal at specific (x, z) position
    glm::vec3 GetNormalAt(float x, float z) const;

private:
    void GenerateMesh(float width, float depth, int segments);
    void SetupBuffers();
    void RegenerateMesh();
    float CalculateHeight(float x, float z) const;

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;
    bool m_MeshInitialized;

    Texture m_Texture;
    std::string m_TexturePath;

    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    float m_Width;
    float m_Depth;
    int m_Segments;
    float m_TextureTiling;
    float m_HeightScale;
    bool m_Wireframe;
    
    // Heightmap type
    enum class HeightmapType { Flat, Noise, Mountain, Valley };
    HeightmapType m_HeightmapType;
    float m_HeightAmplitude;
    float m_HeightFrequency;
};

} // namespace cloth
