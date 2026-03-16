#include "SceneRenderer.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace cloth {

// State caching for performance (reduces OpenGL state changes)
static GLuint g_LastBoundTexture0 = 0;
static GLuint g_LastBoundShader = 0;

void Render(AppState& state, const Application& app) {
    // Initial loading if needed
    if (!state.clothTexturesLoaded) {
        LoadClothTextures(state);
    }

    if (!state.world.IsTexturesLoaded()) {
        state.world.LoadTextures();
    }

    // Check and apply terrain texture changes (only when needed)
    int currentTerrainIndex = state.world.GetCurrentTextureIndex();
    if (currentTerrainIndex >= 0 && (!state.terrainTextureLoaded || (currentTerrainIndex != state.currentTerrainTextureIndex))) {
        LoadTerrainTexture(state);
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

    // === 1. RENDER REFLECTION CUBEMAP (Off-screen) - DISABLED FOR PERFORMANCE ===
    bool shouldUpdateReflection = false;
    /*
    // Always update on first frame
    if (state.reflectionNeedsUpdate) {
        shouldUpdateReflection = true;
    } else if (state.world.IsTexturesLoaded() && state.reflectionCubemap) {
        // Check if camera moved enough to warrant an update (increased threshold for better performance)
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
    */

    // Skip reflection rendering if not needed (CRITICAL for performance)
    if (false && shouldUpdateReflection && state.world.IsTexturesLoaded() && state.reflectionCubemap) {
        glm::vec3 spherePos = state.mirrorSphere.GetPosition();

        // Save viewport
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);

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

            state.reflectionCubemap->EndFaceRender();
        }
        state.reflectionCubemap->EndRender();

        // Restore viewport
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

        // Update tracking variables
        state.lastReflectionUpdatePos = state.camera.GetPosition();
        state.reflectionNeedsUpdate = false;
        
        // Memory barrier to ensure rendering sees updated cubemap
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
    }

    // === 2. MAIN PASS RENDER (On-screen) ===
    // Ensure correct state after reflection rendering
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

    // Always bind the environment map (reflection cubemap)
    if (state.reflectionCubemap && state.reflectionCubemap->GetTextureID() != 0) {
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
    // STATE CACHING: Only bind shader if changed
    if (g_LastBoundShader != state.clothShader.GetID()) {
        state.clothShader.Bind();
        g_LastBoundShader = state.clothShader.GetID();
    }
    
    state.clothShader.SetMat4("u_Model", model);
    state.clothShader.SetMat4("u_View", view);
    state.clothShader.SetMat4("u_Projection", projection);
    state.clothShader.SetVec3("u_LightPos", glm::vec3(5.0f, 10.0f, 5.0f));
    state.clothShader.SetVec3("u_ViewPos", state.camera.GetPosition());
    state.clothShader.SetBool("u_UseTexture", true);

    // Bind particle SSBO for GPU-based cloth rendering
    state.physicsWorld.BindForRendering();

    glDisable(GL_CULL_FACE); // Better for thin cloths
    
    // Render cloths - only render if cloth has fallen close to ground
    // This prevents visual artifacts from particles at initial high positions
    for (size_t i = 0; i < state.clothMeshes.size(); i++) {
        // Skip rendering if cloth is still pinned (waiting to drop)
        if (!state.clothDropped[i]) {
            continue;
        }
        
        // Additional check: only render if cloth has fallen for at least 0.5 seconds
        // This prevents artifacts during initial fall when particles are stretched
        float timeSinceDrop = state.clothDropTimers[i] - state.clothDropStartDelay - (i * state.clothDropDelay);
        if (timeSinceDrop < 0.5f) {
            continue;
        }
        
        if (i < state.clothTextures.size()) {
            // STATE CACHING: Only bind texture if changed
            if (g_LastBoundTexture0 != state.clothTextures[i]) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, state.clothTextures[i]);
                g_LastBoundTexture0 = state.clothTextures[i];
            }
            state.clothShader.SetInt("u_ClothTexture", 0);
            state.clothMeshes[i]->Draw(state.clothShader);
        }
    }
    glEnable(GL_CULL_FACE);

    // Unbind particle buffer
    state.physicsWorld.Unbind();

    state.clothShader.Unbind();
    g_LastBoundShader = 0;  // Reset cache
}

} // namespace cloth
