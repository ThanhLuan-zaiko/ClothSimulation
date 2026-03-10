#include "Renderer.h"
#include <glad/glad.h>

namespace cloth {

Renderer::Renderer() : m_SceneStarted(false) {
    // Create VAO
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);

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
}

Renderer::~Renderer() {
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteBuffers(1, &m_EBO);
}

void Renderer::BeginScene(const Camera& camera) {
    m_ViewProjectionMatrix = camera.GetViewProjectionMatrix();
    m_SceneStarted = true;
}

void Renderer::EndScene() {
    m_SceneStarted = false;
}

void Renderer::DrawLines(const std::vector<glm::vec3>& points, const glm::vec4& color) {
    if (!m_SceneStarted || points.size() < 2) return;

    // Create temporary VAO/VBO for dynamic drawing
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(points.size()));

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void Renderer::DrawPoints(const std::vector<glm::vec3>& points, const glm::vec4& color, float size) {
    if (!m_SceneStarted || points.empty()) return;

    // Create temporary VAO/VBO for dynamic drawing
    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glPointSize(size);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points.size()));

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
}

void Renderer::DrawTriangles(const std::vector<Vertex>& vertices, const Shader& shader) {
    if (!m_SceneStarted || vertices.empty()) return;

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

    glBindVertexArray(0);
}

void Renderer::SetViewport(int width, int height) {
    glViewport(0, 0, width, height);
}

} // namespace cloth
