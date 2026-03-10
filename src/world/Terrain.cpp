#include "Terrain.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>

namespace cloth {

Terrain::Terrain(float width, float depth, int segments)
    : m_VAO(0), m_VBO(0), m_EBO(0), m_Width(width), m_Depth(depth), m_Segments(segments) {
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
            vertex.position.y = 0.0f;
            vertex.position.z = -halfDepth + static_cast<float>(z) * segmentDepth;

            // Normal (pointing up)
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);

            // Texture coordinates - use full texture across terrain
            vertex.texCoord.x = static_cast<float>(x) / static_cast<float>(segments);
            vertex.texCoord.y = static_cast<float>(z) / static_cast<float>(segments);

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

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_Indices.size()),
                   GL_UNSIGNED_INT, 0);
    err = glGetError();
    if (err != GL_NO_ERROR) std::cerr << "Error after glDrawElements: " << err << std::endl;

    glBindVertexArray(0);
}

} // namespace cloth
