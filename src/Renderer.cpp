#include "Renderer.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <iostream>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <Windows.h>

// Helper to get absolute path relative to executable
std::string GetAssetPath(const std::string& relativePath) {
    // Try current directory first
    if (std::filesystem::exists(relativePath)) {
        return relativePath;
    }

    // Try relative to executable
    char pathBuf[4096];
    GetModuleFileNameA(NULL, pathBuf, sizeof(pathBuf));
    std::filesystem::path exePath = std::filesystem::path(pathBuf).parent_path();
    std::filesystem::path assetPath = exePath / relativePath;

    if (std::filesystem::exists(assetPath)) {
        return assetPath.string();
    }

    // Try relative to project root (parent of exe directory)
    std::filesystem::path projectPath = exePath.parent_path().parent_path();
    assetPath = projectPath / relativePath;

    if (std::filesystem::exists(assetPath)) {
        return assetPath.string();
    }

    return relativePath; // fallback
}

void LoadClothTextures(AppState& state) {
    std::cout << "=== Loading cloth textures ===" << std::endl;
    std::vector<std::string> clothTexturePaths = {
        GetAssetPath("assets/cloths/denim_fabric_diff_4k.jpg"),
        GetAssetPath("assets/cloths/fabric_pattern_07_col_1_4k.png"),
        GetAssetPath("assets/cloths/floral_jacquard_diff_4k.jpg")
    };

    for (size_t i = 0; i < clothTexturePaths.size() && i < state.clothMeshes.size(); i++) {
        unsigned int textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        stbi_set_flip_vertically_on_load(1);
        int width, height, channels;
        unsigned char* data = stbi_load(clothTexturePaths[i].c_str(), &width, &height, &channels, 0);

        if (data) {
            GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            stbi_image_free(data);
            std::cout << "Loaded cloth texture " << (i + 1) << ": " << clothTexturePaths[i] << std::endl;
        } else {
            std::cerr << "Failed to load cloth texture: " << clothTexturePaths[i] << std::endl;
        }

        state.clothTextures.push_back(textureID);
    }
    state.clothTexturesLoaded = true;
    std::cout << "=== Cloth textures loading complete ===" << std::endl;
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

        // Delete old texture if exists
        if (state.terrainTextureLoaded && glIsTexture(state.terrainTextureID)) {
            glDeleteTextures(1, &state.terrainTextureID);
        }

        // Create texture
        glGenTextures(1, &state.terrainTextureID);
        glBindTexture(GL_TEXTURE_2D, state.terrainTextureID);

        // Get texture path from world
        const auto& texturePaths = state.world.GetTexturePaths();
        std::string texturePath = texturePaths[newTextureIndex];
        std::cout << "Loading terrain texture: " << texturePath << std::endl;

        // Load image data
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

        // Get VAO info from terrain
        state.terrainVAO = state.world.GetTerrain().GetVAO();
        state.terrainIndexCount = state.world.GetTerrain().GetIndexCount();

        state.currentTerrainTextureIndex = newTextureIndex;
        state.terrainTextureLoaded = true;
    }
}

