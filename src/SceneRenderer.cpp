#include "SceneRenderer.h"
#include "renderer/Renderer.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace cloth {

void Render(AppState& state, const Application& app) {
    // Initial loading if needed
    if (!state.clothTexturesLoaded) {
        LoadClothTextures(state);
    }

    if (!state.world.IsTexturesLoaded()) {
        state.world.LoadTextures();
    }

    // Check and apply terrain texture changes
    LoadTerrainTexture(state);

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
    // Lazy update: Only update reflection when necessary
    bool shouldUpdateReflection = false;

    // Always update on first frame
    if (state.reflectionNeedsUpdate) {
        shouldUpdateReflection = true;
    } else if (state.world.IsTexturesLoaded() && state.reflectionCubemap) {
        // Check if camera moved enough to warrant an update
        float cameraMoveDist = glm::distance(state.camera.GetPosition(), state.lastReflectionUpdatePos);
        if (cameraMoveDist > state.reflectionUpdateThreshold) {
            shouldUpdateReflection = true;
        }

        // Check if terrain texture changed
        int currentTerrainIndex = state.world.GetCurrentTextureIndex();
        if (currentTerrainIndex != state.lastTerrainTextureIndex) {
            shouldUpdateReflection = true;
            state.lastTerrainTextureIndex = currentTerrainIndex;
        }
    }

    if (shouldUpdateReflection && state.world.IsTexturesLoaded() && state.reflectionCubemap) {
        glm::vec3 spherePos = state.mirrorSphere.GetPosition();

        state.reflectionCubemap->BeginRender();

        for (unsigned int face = 0; face < 6; face++) {
            state.reflectionCubemap->BeginFaceRender(face);

            glm::mat4 reflectionView = state.reflectionCubemap->GetViewMatrix(face, spherePos);
            glm::mat4 reflectionProjection = state.reflectionCubemap->GetProjectionMatrix();

            // Clear face
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);

            // 1a. Render skybox into reflection (center on sphere position)
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST); // Skybox doesn't need depth test
            state.skyboxShader.Bind();
            state.skyboxShader.SetMat4("u_View", reflectionView);
            state.skyboxShader.SetMat4("u_Projection", reflectionProjection);

            GLuint skyboxID = state.world.GetSkybox().GetTextureID();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxID);
            state.skyboxShader.SetInt("u_Skybox", 0);
            state.world.GetSkybox().Draw();
            state.skyboxShader.Unbind();
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);

            // 1b. Render terrain into reflection (with occlusion culling)
            // Occlusion Culling: Only render terrain if this face is pointing downward or horizontal
            // Face directions: 0=+X, 1=-X, 2=+Y (up), 3=-Y (down), 4=+Z, 5=-Z
            bool shouldRenderTerrain = (face != 2); // Don't render terrain for +Y (sky) face

            if (shouldRenderTerrain) {
                glDisable(GL_CULL_FACE);
                glActiveTexture(GL_TEXTURE0);
                state.terrainShader.Bind();
                state.terrainShader.SetMat4("u_Model", model);
                state.terrainShader.SetMat4("u_View", reflectionView);
                state.terrainShader.SetMat4("u_Projection", reflectionProjection);
                state.world.GetTerrain().Draw(state.terrainShader);
                state.terrainShader.Unbind();
            }

            // Note: Not rendering cloths into reflection for better performance
            // Cloths are still rendered in the main pass and will be visible on the sphere

            state.reflectionCubemap->EndFaceRender();
        }
        state.reflectionCubemap->EndRender();

        // Update tracking variables
        state.lastReflectionUpdatePos = state.camera.GetPosition();
        state.reflectionNeedsUpdate = false;
    }

    // === 2. MAIN PASS RENDER (On-screen) ===
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // --- Draw skybox ---
    glDisable(GL_CULL_FACE);
    state.skyboxShader.Bind();
    state.skyboxShader.SetMat4("u_View", view);
    state.skyboxShader.SetMat4("u_Projection", projection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, state.world.GetSkybox().GetTextureID());
    state.skyboxShader.SetInt("u_Skybox", 0);
    state.world.GetSkybox().Draw();
    state.skyboxShader.Unbind();
    glEnable(GL_CULL_FACE);

    // --- Draw terrain ---
    state.terrainShader.Bind();
    state.terrainShader.SetMat4("u_Model", model);
    state.terrainShader.SetMat4("u_View", view);
    state.terrainShader.SetMat4("u_Projection", projection);
    state.world.GetTerrain().Draw(state.terrainShader);
    state.terrainShader.Unbind();

    // --- Draw Mirror Sphere ---
    state.sphereShader.Bind();
    glm::mat4 sphereModel = glm::translate(glm::mat4(1.0f), state.mirrorSphere.GetPosition());
    state.sphereShader.SetMat4("u_Model", sphereModel);
    state.sphereShader.SetMat4("u_View", view);
    state.sphereShader.SetMat4("u_Projection", projection);
    state.sphereShader.SetVec3("u_ViewPos", state.camera.GetPosition());
    state.sphereShader.SetVec3("u_Color", state.mirrorSphere.GetColor());

    // Bind the environment map (reflection cubemap)
    if (state.reflectionCubemap) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, state.reflectionCubemap->GetTextureID());
        state.sphereShader.SetInt("u_EnvironmentMap", 0);
    }

    // Ensure normal render states are enabled
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    state.mirrorSphere.Draw();
    state.sphereShader.Unbind();

    // --- Draw cloths ---
    state.clothShader.Bind();
    state.clothShader.SetMat4("u_Model", model);
    state.clothShader.SetMat4("u_View", view);
    state.clothShader.SetMat4("u_Projection", projection);
    state.clothShader.SetVec3("u_LightPos", glm::vec3(5.0f, 10.0f, 5.0f));
    state.clothShader.SetVec3("u_ViewPos", state.camera.GetPosition());
    state.clothShader.SetBool("u_UseTexture", true);

    // Bind particle SSBO for GPU-based cloth rendering
    state.physicsWorld.BindForRendering();

    glDisable(GL_CULL_FACE); // Better for thin cloths
    for (size_t i = 0; i < state.clothMeshes.size(); i++) {
        if (i < state.clothTextures.size()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, state.clothTextures[i]);
            state.clothShader.SetInt("u_ClothTexture", 0);
            state.clothMeshes[i]->Draw(*reinterpret_cast<Renderer*>(nullptr), state.clothShader);
        }
    }
    glEnable(GL_CULL_FACE);

    // Unbind particle buffer
    state.physicsWorld.Unbind();

    state.clothShader.Unbind();
}

} // namespace cloth
