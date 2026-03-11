#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace cloth {

class Skybox {
public:
    Skybox();
    ~Skybox();

    void Initialize();
    void LoadTextures(const std::string& folderPath);
    void Draw() const;

    unsigned int GetVAO() const { return m_VAO; }
    unsigned int GetVBO() const { return m_VBO; }
    unsigned int GetTextureID() const { return m_CubemapTexture; }

private:
    void SetupBuffers();
    void LoadCubemap(const std::vector<std::string>& faces);
    void CreateGradientSky();

    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_CubemapTexture;
    bool m_Initialized;
};

} // namespace cloth
