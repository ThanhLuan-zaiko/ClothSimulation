#include "SceneRenderer.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

extern std::string GetAssetPath(const std::string& relativePath);

void LoadClothTextures(AppState& state) {
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
            std::cout << "Loaded cloth texture: " << clothTexturePaths[i] << std::endl;
        } else {
            std::cerr << "Failed to load cloth texture: " << clothTexturePaths[i] << std::endl;
        }

        state.clothTextures.push_back(textureID);
    }
    state.clothTexturesLoaded = true;
}

void Render(AppState& state, const Application& app) {
    // Initial loading if needed
    if (!state.clothTexturesLoaded) {
        LoadClothTextures(state);
    }

    if (!state.world.IsTexturesLoaded()) {
        state.world.LoadTextures();
    }

    // Get current window/viewport size
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    float aspectRatio = static_cast<float>(viewport[2]) / static_cast<float>(viewport[3]);
    state.camera.SetProjection(aspectRatio);

    // Common matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = state.camera.GetViewMatrix();
    glm::mat4 projection = state.camera.GetProjectionMatrix();

    // === 1. RENDER REFLECTION CUBEMAP (Off-screen) ===
    // OPTIMIZATION: Only update the reflection cubemap if something changed (like terrain texture)
    // Since the terrain is static, we don't need to re-render it 6 times every frame!
    if (state.reflectionNeedsUpdate && state.world.IsTexturesLoaded()) {
        glm::vec3 spherePos = state.mirrorSphere.GetPosition();
        state.reflectionCubemap.BeginRender();
        for (unsigned int face = 0; face < 6; face++) {
            state.reflectionCubemap.BeginFaceRender(face);

            glm::mat4 reflectionView = state.reflectionCubemap.GetViewMatrix(face, spherePos);
            glm::mat4 reflectionProjection = state.reflectionCubemap.GetProjectionMatrix();

            // Render terrain into reflection
            state.terrainShader.Bind();
            state.terrainShader.SetMat4("u_Model", model);
            state.terrainShader.SetMat4("u_View", reflectionView);
            state.terrainShader.SetMat4("u_Projection", reflectionProjection);
            state.world.GetTerrain().Draw(state.terrainShader);
            state.terrainShader.Unbind();

            state.reflectionCubemap.EndFaceRender();
        }
        state.reflectionCubemap.EndRender();
        
        // Reset the update flag - we have a valid cubemap now
        state.reflectionNeedsUpdate = false;
        std::cout << "Reflection cubemap updated (Optimization triggered)" << std::endl;
    }

    // IMPORTANT: Restore main viewport explicitly
    glViewport(0, 0, viewport[2], viewport[3]);

    // === 2. MAIN PASS RENDER (On-screen) ===
    glClearColor(0.5f, 0.7f, 0.9f, 1.0f);  // Light blue sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);

    // --- Draw terrain ---
    state.terrainShader.Bind();
    state.terrainShader.SetMat4("u_Model", model);
    state.terrainShader.SetMat4("u_View", view);
    state.terrainShader.SetMat4("u_Projection", projection);
    state.world.GetTerrain().Draw(state.terrainShader);
    state.terrainShader.Unbind();

    glEnable(GL_CULL_FACE);

    // --- Draw cloths ---
    state.clothShader.Bind();
    state.clothShader.SetMat4("u_Model", model);
    state.clothShader.SetMat4("u_View", view);
    state.clothShader.SetMat4("u_Projection", projection);
    state.clothShader.SetVec3("u_LightPos", glm::vec3(5.0f, 10.0f, 5.0f));
    state.clothShader.SetVec3("u_ViewPos", state.camera.GetPosition());
    state.clothShader.SetBool("u_UseTexture", true);

    for (size_t i = 0; i < state.clothMeshes.size(); i++) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state.clothTextures[i]);
        state.clothShader.SetInt("u_ClothTexture", 0);
        state.clothMeshes[i]->Draw(*reinterpret_cast<Renderer*>(nullptr), state.clothShader);
    }

    state.clothShader.Unbind();
}
