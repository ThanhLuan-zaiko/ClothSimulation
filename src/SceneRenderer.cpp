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

    // Load world textures FIRST (including skybox)
    if (!state.world.IsTexturesLoaded()) {
        state.world.LoadTextures();
    }

    // Ensure skybox texture is loaded
    static bool skyboxChecked = false;
    if (!skyboxChecked) {
        if (state.world.GetSkybox().GetTextureID() == 0) {
            // Skybox not ready yet - just clear with sky color and skip rendering this frame
            glClearColor(0.6f, 0.75f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            return;
        }
        skyboxChecked = true;
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

    // === MAIN PASS RENDER ===
    // Ensure correct state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // --- Draw skybox FIRST (immediately, before any other rendering) ---
    // Clear with sky color
    glClearColor(0.6f, 0.75f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Draw skybox with depth test disabled and depth mask off
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    // Bind skybox shader and set uniforms
    state.skyboxShader.Bind();
    state.skyboxShader.SetMat4("u_View", view);
    state.skyboxShader.SetMat4("u_Projection", projection);
    
    // Bind skybox texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, state.world.GetSkybox().GetTextureID());
    state.skyboxShader.SetInt("u_Skybox", 0);
    
    // Draw skybox
    state.world.GetSkybox().Draw();
    
    // Unbind shader
    state.skyboxShader.Unbind();
    
    // Re-enable states
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    // === 1. RENDER REFLECTION CUBEMAP (Off-screen) ===
    static int frameCount = 0;
    frameCount++;

    bool shouldUpdateReflection = false;

    // Update reflection after first 30 frames (reduced from 60 for faster startup)
    if (frameCount > 30 && state.world.IsTexturesLoaded() && state.reflectionCubemap && state.reflectionNeedsUpdate) {
        // Check if camera moved enough to warrant an update (reduced threshold for smoother reflection)
        float cameraMoveDist = glm::distance(state.camera.GetPosition(), state.lastReflectionUpdatePos);

        // Dynamic threshold: smaller threshold for better quality (reduced from 3.0 to 1.5)
        float sphereDistance = glm::distance(state.camera.GetPosition(), state.mirrorSphere.GetPosition());
        float dynamicThreshold = 1.5f * (1.0f + sphereDistance * 0.1f);

        if (cameraMoveDist > dynamicThreshold) {
            shouldUpdateReflection = true;
        }

        // Check if terrain texture changed
        int currentTerrainIndex = state.world.GetCurrentTextureIndex();
        if (currentTerrainIndex != state.lastTerrainTextureIndex) {
            shouldUpdateReflection = true;
            state.lastTerrainTextureIndex = currentTerrainIndex;
        }

        // Check if camera rotation changed significantly (view direction matters for reflection)
        glm::vec3 currentViewDir = state.camera.GetFront();
        glm::vec3 lastViewDir = state.lastReflectionViewDir;
        float viewAngleChange = glm::acos(glm::clamp(glm::dot(currentViewDir, lastViewDir), -1.0f, 1.0f));
        if (viewAngleChange > 0.05f) { // Reduced threshold from 0.1 to 0.05 for smoother updates
            shouldUpdateReflection = true;
        }

        // Render reflection cubemap when needed
        if (shouldUpdateReflection) {
            // Save current framebuffer binding
            GLint currentFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);

            glm::vec3 spherePos = state.mirrorSphere.GetPosition();

            // Save viewport
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);

            state.reflectionCubemap->BeginRender();

        for (unsigned int face = 0; face < 6; face++) {
            state.reflectionCubemap->BeginFaceRender(face);

            glm::mat4 reflectionView = state.reflectionCubemap->GetViewMatrix(face, spherePos);
            glm::mat4 reflectionProjection = state.reflectionCubemap->GetProjectionMatrix();

            // Clear face with sky color to avoid black edges
            glClearColor(0.6f, 0.75f, 1.0f, 1.0f);
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

        // Restore framebuffer binding
        glBindFramebuffer(GL_FRAMEBUFFER, currentFBO);

        // Restore viewport
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

        // Update tracking variables
        state.lastReflectionUpdatePos = state.camera.GetPosition();
        state.lastReflectionViewDir = state.camera.GetFront();
        state.reflectionNeedsUpdate = false;

        // Memory barrier to ensure rendering sees updated cubemap
        glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
        } // End shouldUpdateReflection
    } // End frameCount > 30

    // Continue MAIN PASS RENDER - Draw terrain, sphere, cloths
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

    // PBR material properties for mirror-like surface (configurable)
    state.sphereShader.SetFloat("u_Metallic", state.sphereMetallic);
    state.sphereShader.SetFloat("u_Roughness", state.sphereRoughness);

    // HDR settings (configurable) - increased exposure for brighter sky reflection
    state.sphereShader.SetFloat("u_Exposure", state.reflectionExposure * 2.0f); // Boost exposure by 100% for better contrast
    state.sphereShader.SetInt("u_UseHDR", state.reflectionCubemap && state.reflectionCubemap->IsHDR() ? 1 : 0);

    // LOD bias based on distance from camera to sphere - MINIMAL for sharp reflection
    float lodBias = 0.0f;
    if (state.reflectionLOD) {
        float sphereDistance = glm::distance(state.camera.GetPosition(), state.mirrorSphere.GetPosition());
        // Reduced from 0.3f to 0.05f for much sharper reflection (80% reduction)
        lodBias = glm::clamp((sphereDistance - 5.0f) * 0.05f, 0.0f, 0.3f); // Max 0.3 instead of 2.0
    }
    state.sphereShader.SetFloat("u_LODBias", lodBias);

    // Bind environment map (reflection cubemap or skybox as fallback)
    glActiveTexture(GL_TEXTURE0);
    if (frameCount > 60 && state.reflectionCubemap && state.reflectionCubemap->GetTextureID() != 0) {
        glBindTexture(GL_TEXTURE_CUBE_MAP, state.reflectionCubemap->GetTextureID());
    } else {
        glBindTexture(GL_TEXTURE_CUBE_MAP, state.world.GetSkybox().GetTextureID());
    }
    state.sphereShader.SetInt("u_EnvironmentMap", 0);

    // Ensure normal render states are enabled
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    state.mirrorSphere.Draw();
    state.sphereShader.Unbind();

    // --- Draw cloths ---
    if (state.clothTexturesLoaded && !state.clothMeshes.empty()) {
        state.clothShader.Bind();
        state.clothShader.SetMat4("u_Model", model);
        state.clothShader.SetMat4("u_View", view);
        state.clothShader.SetMat4("u_Projection", projection);
        state.clothShader.SetVec3("u_LightPos", glm::vec3(5.0f, 10.0f, 5.0f));
        state.clothShader.SetVec3("u_ViewPos", state.camera.GetPosition());
        state.clothShader.SetBool("u_UseTexture", true);
        state.clothShader.SetInt("u_Wireframe", 0);
        state.clothShader.SetInt("u_Interactive", state.isInteracting ? 1 : 0);
        state.clothShader.SetVec3("u_Color", glm::vec3(1.0f, 1.0f, 1.0f));

        // Ensure correct render states BEFORE rendering cloths
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL); 
        glDepthMask(GL_TRUE); 
        
        // Disable back-face culling to make the cloth visible from both sides
        glDisable(GL_CULL_FACE);

        // Render each cloth
        for (size_t i = 0; i < state.clothMeshes.size(); i++) {
            if (i < state.clothTextures.size() && glIsTexture(state.clothTextures[i])) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, state.clothTextures[i]);
                state.clothShader.SetInt("u_ClothTexture", 0);

                state.clothMeshes[i]->Draw(state.clothShader);

                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
        
        // Re-enable culling for subsequent draw calls
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        state.clothShader.Unbind();
    }

    // Unbind physics world
    state.physicsWorld.Unbind();
}

} // namespace cloth
