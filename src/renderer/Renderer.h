#pragma once

#include "Shader.h"
#include "Camera.h"
#include "utils/Types.h"
#include <glm/glm.hpp>
#include <vector>

namespace cloth {

class Renderer {
public:
    Renderer();
    ~Renderer();

    void BeginScene(const Camera& camera);
    void EndScene();

    // Draw methods
    void DrawLines(const std::vector<glm::vec3>& points, const glm::vec4& color);
    void DrawPoints(const std::vector<glm::vec3>& points, const glm::vec4& color, float size = 5.0f);
    void DrawTriangles(const std::vector<Vertex>& vertices, const Shader& shader);
    void DrawTerrain(const class Terrain& terrain, const Shader& shader);

    void SetViewport(int width, int height);

private:
    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_EBO;
    
    glm::mat4 m_ViewProjectionMatrix;
    bool m_SceneStarted;
};

} // namespace cloth
