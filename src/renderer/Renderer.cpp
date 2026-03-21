#include "Renderer.h"
#include "world/Terrain.h"
#include "AppState.h"
#include <future>  // For async texture loading

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

#include <glad/glad.h>
#include <stb_image.h>
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace cloth {

Renderer::Renderer() : m_ViewProjectionMatrix(1.0f), m_SceneStarted(false) {
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

void Renderer::DrawTerrain(const Terrain& terrain, const Shader& shader) {
    terrain.Draw(shader);
}

// === Texture Loading Utilities ===

// Helper to get absolute path relative to executable
static std::string GetAssetPath(const std::string& relativePath) {
    if (std::filesystem::exists(relativePath)) {
        return relativePath;
    }

    char pathBuf[4096];
    GetModuleFileNameA(NULL, pathBuf, sizeof(pathBuf));
    std::filesystem::path exePath = std::filesystem::path(pathBuf).parent_path();
    std::filesystem::path assetPath = exePath / relativePath;

    if (std::filesystem::exists(assetPath)) {
        return assetPath.string();
    }

    std::filesystem::path projectPath = exePath.parent_path().parent_path();
    assetPath = projectPath / relativePath;

    if (std::filesystem::exists(assetPath)) {
        return assetPath.string();
    }

    return relativePath;
}

void LoadClothTextures(AppState& state) {
    std::vector<std::string> clothTexturePaths = {
        GetAssetPath("assets/cloths/denim_fabric_diff_4k.jpg"),
        GetAssetPath("assets/cloths/fabric_pattern_07_col_1_4k.png"),
        GetAssetPath("assets/cloths/floral_jacquard_diff_4k.jpg")
    };

    // Load textures synchronously
    for (size_t i = 0; i < clothTexturePaths.size(); i++) {
        stbi_set_flip_vertically_on_load(1);
        int width, height, channels;
        unsigned char* data = stbi_load(clothTexturePaths[i].c_str(), &width, &height, &channels, 0);

        unsigned int textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        if (data) {
            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 8);
            stbi_image_free(data);
            std::cout << "[ClothTexture " << i << "] Loaded: " << clothTexturePaths[i] 
                      << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;
        } else {
            std::cerr << "[ClothTextures] Failed to load: " << clothTexturePaths[i] << std::endl;
            // Create a default 1x1 white texture as fallback
            unsigned char whitePixel[] = {255, 255, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, whitePixel);
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        state.clothTextures.push_back(textureID);
    }

    state.clothTexturesLoaded = true;
}

void LoadTerrainTexture(AppState& state) {
    int newTextureIndex = state.world.GetCurrentTextureIndex();
    bool needReload = !state.terrainTextureLoaded || (newTextureIndex != state.currentTerrainTextureIndex);

    if (needReload && newTextureIndex >= 0) {
        if (!state.terrainTextureLoaded) {
            std::cout << "=== Creating terrain texture ===" << std::endl;
        } else {
            std::cout << "=== Switching to terrain texture index " << newTextureIndex << " ===" << std::endl;
        }

        if (state.terrainTextureLoaded && glIsTexture(state.terrainTextureID)) {
            glDeleteTextures(1, &state.terrainTextureID);
        }

        glGenTextures(1, &state.terrainTextureID);
        glBindTexture(GL_TEXTURE_2D, state.terrainTextureID);

        const auto& texturePaths = state.world.GetTexturePaths();
        std::string texturePath = texturePaths[newTextureIndex];
        std::cout << "Loading terrain texture: " << texturePath << std::endl;

        stbi_set_flip_vertically_on_load(1);
        int width, height, channels;
        unsigned char* data = stbi_load(texturePath.c_str(), &width, &height, &channels, 0);

        if (data) {
            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            stbi_image_free(data);
            std::cout << "Terrain texture loaded: " << width << "x" << height << std::endl;
        } else {
            std::cerr << "Failed to load terrain texture: " << texturePath << std::endl;
        }

        state.terrainVAO = state.world.GetTerrain().GetVAO();
        state.terrainIndexCount = state.world.GetTerrain().GetIndexCount();

        state.currentTerrainTextureIndex = newTextureIndex;
        state.terrainTextureLoaded = true;
    }
}

} // namespace cloth