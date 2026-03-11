#include "Terrain.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/noise.hpp>
#include <iostream>
#include <cmath>

namespace cloth {

Terrain::Terrain(float width, float depth, int segments, float textureTiling)
    : m_VAO(0), m_VBO(0), m_EBO(0), m_Width(width), m_Depth(depth), m_Segments(segments), 
      m_TextureTiling(textureTiling), m_HeightScale(1.0f), m_Wireframe(false),
      m_HeightmapType(HeightmapType::Flat), m_HeightAmplitude(2.0f), m_HeightFrequency(0.5f) {
    GenerateMesh(width, depth, segments);
    SetupBuffers();
}

Terrain::~Terrain() {
    if (m_VAO != 0) {
        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
    }
}

float Terrain::CalculateHeight(float x, float z) {
    switch (m_HeightmapType) {
        case HeightmapType::Flat:
            return 0.0f;
            
        case HeightmapType::Noise: {
            float scale = m_HeightFrequency;
            float height = glm::simplex(glm::vec2(x * scale, z * scale));
            height = height * m_HeightAmplitude;
            return height;
        }
        
        case HeightmapType::Mountain: {
            float dist = std::sqrt(x * x + z * z);
            float maxDist = std::sqrt(m_Width * m_Width + m_Depth * m_Depth) * 0.5f;
            float height = std::max(0.0f, (maxDist - dist) / maxDist);
            height = std::pow(height, 2.0f) * m_HeightAmplitude;
            
            // Add some noise
            float noise = glm::simplex(glm::vec2(x * 0.3f, z * 0.3f)) * 0.5f;
            return height + noise;
        }
        
        case HeightmapType::Valley: {
            float dist = std::abs(x);
            float maxDist = m_Width * 0.5f;
            float height = (dist / maxDist) * m_HeightAmplitude;
            
            // Add noise
            float noise = glm::simplex(glm::vec2(x * 0.2f, z * 0.2f)) * 0.3f;
            return height + noise;
        }
        
        default:
            return 0.0f;
    }
}

void Terrain::GenerateMesh(float width, float depth, int segments) {
    m_Vertices.clear();
    m_Indices.clear();

    float halfWidth = width * 0.5f;
    float halfDepth = depth * 0.5f;
    float segmentWidth = width / static_cast<float>(segments);
    float segmentDepth = depth / static_cast<float>(segments);

    // Generate vertices
    for (int z = 0; z <= segments; ++z) {
        for (int x = 0; x <= segments; ++x) {
            Vertex vertex;

            // Position
            vertex.position.x = -halfWidth + static_cast<float>(x) * segmentWidth;
            vertex.position.z = -halfDepth + static_cast<float>(z) * segmentDepth;
            
            // Calculate height using heightmap
            vertex.position.y = CalculateHeight(vertex.position.x, vertex.position.z) * m_HeightScale;

            // Calculate normal using finite differences
            float delta = segmentWidth * 0.1f;
            float hL = CalculateHeight(vertex.position.x - delta, vertex.position.z) * m_HeightScale;
            float hR = CalculateHeight(vertex.position.x + delta, vertex.position.z) * m_HeightScale;
            float hD = CalculateHeight(vertex.position.x, vertex.position.z - delta) * m_HeightScale;
            float hU = CalculateHeight(vertex.position.x, vertex.position.z + delta) * m_HeightScale;
            
            vertex.normal = glm::normalize(glm::vec3(hL - hR, 2.0f * delta, hD - hU));

            // Texture coordinates - apply tiling factor to repeat texture
            vertex.texCoord.x = (static_cast<float>(x) / static_cast<float>(segments)) * m_TextureTiling;
            vertex.texCoord.y = (static_cast<float>(z) / static_cast<float>(segments)) * m_TextureTiling;

            m_Vertices.push_back(vertex);
        }
    }

    // Generate indices
    for (int z = 0; z < segments; ++z) {
        for (int x = 0; x < segments; ++x) {
            unsigned int topLeft = z * (segments + 1) + x;
            unsigned int topRight = topLeft + 1;
            unsigned int bottomLeft = (z + 1) * (segments + 1) + x;
            unsigned int bottomRight = bottomLeft + 1;

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

void Terrain::SetupBuffers() {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    // Bind VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_Vertices.size() * sizeof(Vertex),
                 m_Vertices.data(), GL_STATIC_DRAW);

    // Bind EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_Indices.size() * sizeof(unsigned int),
                 m_Indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, position)));

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, normal)));

    // Texture coordinate attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, texCoord)));

    glBindVertexArray(0);

    std::cout << "Terrain mesh created: " << m_Vertices.size() << " vertices, "
              << m_Indices.size() << " indices" << std::endl;
}

void Terrain::LoadTexture(const std::string& path) {
    m_Texture = Texture(path);
    m_TexturePath = path;
}

void Terrain::SetTexture(const Texture& texture) {
    m_Texture = texture;
}

void Terrain::BindTexture(unsigned int slot) const {
    m_Texture.Bind(slot);
}

void Terrain::UnbindTexture() const {
    m_Texture.Unbind();
}

void Terrain::Draw(const Shader& shader) const {
    // Debug output
    static bool firstDraw = true;
    if (firstDraw) {
        std::cout << "Terrain::Draw - Texture ID: " << m_Texture.GetID()
                  << ", Size: " << m_Texture.GetWidth() << "x" << m_Texture.GetHeight()
                  << ", VAO: " << m_VAO << std::endl;
        firstDraw = false;
    }

    GLenum err;

    // Bind texture to unit 0
    glActiveTexture(GL_TEXTURE0);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glActiveTexture: " << err << std::endl;

    BindTexture(0);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glBindTexture: " << err << std::endl;

    // Bind VAO and re-enable attributes to be safe
    glBindVertexArray(m_VAO);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glBindVertexArray: " << err << std::endl;

    // Re-enable vertex attributes (in case they were disabled)
    glEnableVertexAttribArray(0);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glEnableVertexAttribArray(0): " << err << std::endl;

    glEnableVertexAttribArray(1);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glEnableVertexAttribArray(1): " << err << std::endl;

    glEnableVertexAttribArray(2);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glEnableVertexAttribArray(2): " << err << std::endl;

    // Wireframe mode
    if (m_Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()),
                   GL_UNSIGNED_INT, 0);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glDrawElements: " << err << std::endl;

    // Reset polygon mode
    if (m_Wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glBindVertexArray(0);
}

void Terrain::RegenerateMesh() {
    GenerateMesh(m_Width, m_Depth, m_Segments);
    
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_Vertices.size() * sizeof(Vertex), m_Vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Terrain::GenerateFlatHeightmap() {
    m_HeightmapType = HeightmapType::Flat;
    RegenerateMesh();
    std::cout << "Terrain: Flat heightmap" << std::endl;
}

void Terrain::GenerateNoiseHeightmap(float amplitude, float frequency) {
    m_HeightmapType = HeightmapType::Noise;
    m_HeightAmplitude = amplitude;
    m_HeightFrequency = frequency;
    RegenerateMesh();
    std::cout << "Terrain: Noise heightmap (amplitude=" << amplitude << ", frequency=" << frequency << ")" << std::endl;
}

void Terrain::GenerateMountainHeightmap(float peakHeight) {
    m_HeightmapType = HeightmapType::Mountain;
    m_HeightAmplitude = peakHeight;
    RegenerateMesh();
    std::cout << "Terrain: Mountain heightmap (peakHeight=" << peakHeight << ")" << std::endl;
}

void Terrain::GenerateValleyHeightmap(float depth) {
    m_HeightmapType = HeightmapType::Valley;
    m_HeightAmplitude = depth;
    RegenerateMesh();
    std::cout << "Terrain: Valley heightmap (depth=" << depth << ")" << std::endl;
}

} // namespace cloth
