#include <iostream>
#include <filesystem>
#include <glm/glm.hpp>
#include <Windows.h>

#include "core/Application.h"
#include "AppState.h"
#include "InputHandler.h"
#include "SceneRenderer.h"

using namespace cloth;

// Global application pointer
static Application* g_App = nullptr;

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

int main() {
    std::cout << "=== Cloth Simulation ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD/Arrows - Move camera" << std::endl;
    std::cout << "  Shift + WASD - Fast camera (sprint)" << std::endl;
    std::cout << "  Mouse - Rotate camera (click and drag)" << std::endl;
    std::cout << "  +/- or PageUp/PageDown - Zoom in/out" << std::endl;
    std::cout << "  Space - Reset cloth" << std::endl;
    std::cout << "  F - Toggle terrain wireframe" << std::endl;
    std::cout << "  T/Shift+T - Change terrain texture" << std::endl;
    std::cout << "  1-4 - Select terrain texture directly" << std::endl;
    std::cout << "  Escape - Exit" << std::endl;
    std::cout << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  - Mirror sphere at center reflects the environment" << std::endl;
    std::cout << "  - Mountain terrain with heightmap" << std::endl;
    std::cout << "  - Skybox with gradient sky" << std::endl;
    std::cout << "  - Pillars and rocks" << std::endl;
    std::cout << "  - 3 textured cloths draping over the sphere" << std::endl;
    std::cout << std::endl;
    std::cout << "Tip: Click and drag mouse to rotate camera and look around the mirror sphere!" << std::endl;
    std::cout << std::endl;

    // Create application
    Application app;
    g_App = &app;

    // Create application state
    AppState state;

    // Initialize OpenGL-dependent objects (GL context is now ready)
    state.InitializeGL();

    // Load terrain shader from file
    state.terrainShader = Shader::CreateFromFile(
        GetAssetPath("shaders/terrain/terrain.vert"),
        GetAssetPath("shaders/terrain/terrain.frag")
    );

    // Load sphere shader from file
    state.sphereShader = Shader::CreateFromFile(
        GetAssetPath("shaders/sphere/sphere.vert"),
        GetAssetPath("shaders/sphere/sphere.frag")
    );

    // Load skybox shader from file
    state.skyboxShader = Shader::CreateFromFile(
        GetAssetPath("shaders/skybox/skybox.vert"),
        GetAssetPath("shaders/skybox/skybox.frag")
    );

    // Initialize world (scan textures, create skybox)
    state.world.Initialize(GetAssetPath("assets/textures/lands/"));

    // Initialize world OpenGL resources (terrain mesh, etc.)
    state.world.InitializeGL();

    // Generate terrain heightmap AFTER OpenGL initialization
    state.world.GetTerrain().GenerateMountainHeightmap(3.0f);
    state.world.GetTerrain().SetHeightScale(1.0f);

    // Set terrain reference in GPU physics world for collision
    state.physicsWorld.SetTerrain(&state.world.GetTerrain());

    // Pre-load textures
    state.world.LoadTextures();
    LoadClothTextures(state);

    // Create 3 cloths that will drop gracefully onto the mirror sphere
    // Each cloth starts high above and falls down with a delay
    // For GPU physics, we initialize cloth data directly on GPU
    
    // Track cumulative particle offsets for correct unpinning
    size_t cumulativeParticleOffset = 0;

    // Cloth 1 - First to drop (starts at y=25)
    ClothConfig clothConfig1;
    clothConfig1.widthSegments = 60;
    clothConfig1.heightSegments = 60;
    clothConfig1.segmentLength = 0.12f;
    clothConfig1.startX = -3.6f;
    clothConfig1.startY = 25.0f;
    clothConfig1.startZ = -3.6f;

    size_t cloth1Offset = state.physicsWorld.InitializeCloth(
        clothConfig1.widthSegments, clothConfig1.heightSegments,
        clothConfig1.startX, clothConfig1.startY, clothConfig1.startZ,
        clothConfig1.segmentLength, true  // Start pinned
    );

    // Create ClothMesh for rendering (will read from GPU buffer)
    Cloth* dummyCloth1 = new Cloth(state.physicsWorld, clothConfig1);
    ClothMesh* clothMesh1 = new ClothMesh(*dummyCloth1);
    clothMesh1->SetGPUBased(true);
    clothMesh1->SetParticleBuffer(&state.physicsWorld.GetParticleBuffer(), cloth1Offset);
    state.cloths.push_back(dummyCloth1);
    state.clothMeshes.push_back(clothMesh1);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    size_t cloth1Count = (clothConfig1.widthSegments + 1) * (clothConfig1.heightSegments + 1);
    state.clothParticleCounts.push_back(cloth1Count);
    state.clothParticleOffsets.push_back(cumulativeParticleOffset);  // Track offset
    cumulativeParticleOffset += cloth1Count;

    // Cloth 2 - Second to drop (starts at y=28, offset)
    ClothConfig clothConfig2;
    clothConfig2.widthSegments = 55;
    clothConfig2.heightSegments = 55;
    clothConfig2.segmentLength = 0.12f;
    clothConfig2.startX = -3.3f;
    clothConfig2.startY = 28.0f;
    clothConfig2.startZ = -3.3f;

    size_t cloth2Offset = state.physicsWorld.InitializeCloth(
        clothConfig2.widthSegments, clothConfig2.heightSegments,
        clothConfig2.startX, clothConfig2.startY, clothConfig2.startZ,
        clothConfig2.segmentLength, true  // Start pinned
    );

    Cloth* dummyCloth2 = new Cloth(state.physicsWorld, clothConfig2);
    ClothMesh* clothMesh2 = new ClothMesh(*dummyCloth2);
    clothMesh2->SetGPUBased(true);
    clothMesh2->SetParticleBuffer(&state.physicsWorld.GetParticleBuffer(), cloth2Offset);
    state.cloths.push_back(dummyCloth2);
    state.clothMeshes.push_back(clothMesh2);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    size_t cloth2Count = (clothConfig2.widthSegments + 1) * (clothConfig2.heightSegments + 1);
    state.clothParticleCounts.push_back(cloth2Count);
    state.clothParticleOffsets.push_back(cumulativeParticleOffset);  // Track offset
    cumulativeParticleOffset += cloth2Count;

    // Cloth 3 - Third to drop (starts at y=31, offset)
    ClothConfig clothConfig3;
    clothConfig3.widthSegments = 55;
    clothConfig3.heightSegments = 55;
    clothConfig3.segmentLength = 0.12f;
    clothConfig3.startX = -3.3f;
    clothConfig3.startY = 31.0f;
    clothConfig3.startZ = -3.3f;

    size_t cloth3Offset = state.physicsWorld.InitializeCloth(
        clothConfig3.widthSegments, clothConfig3.heightSegments,
        clothConfig3.startX, clothConfig3.startY, clothConfig3.startZ,
        clothConfig3.segmentLength, true  // Start pinned
    );

    Cloth* dummyCloth3 = new Cloth(state.physicsWorld, clothConfig3);
    ClothMesh* clothMesh3 = new ClothMesh(*dummyCloth3);
    clothMesh3->SetGPUBased(true);
    clothMesh3->SetParticleBuffer(&state.physicsWorld.GetParticleBuffer(), cloth3Offset);
    state.cloths.push_back(dummyCloth3);
    state.clothMeshes.push_back(clothMesh3);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    size_t cloth3Count = (clothConfig3.widthSegments + 1) * (clothConfig3.heightSegments + 1);
    state.clothParticleCounts.push_back(cloth3Count);
    state.clothParticleOffsets.push_back(cumulativeParticleOffset);  // Track offset
    cumulativeParticleOffset += cloth3Count;
    
    // Re-set particle buffer references after all cloths are created
    clothMesh1->SetParticleBuffer(&state.physicsWorld.GetParticleBuffer(), 0);
    clothMesh2->SetParticleBuffer(&state.physicsWorld.GetParticleBuffer(), cloth1Count);
    clothMesh3->SetParticleBuffer(&state.physicsWorld.GetParticleBuffer(), cloth1Count + cloth2Count);

    // Set update callback
    app.SetUpdateCallback([&](float deltaTime) {
        HandleInput(state, deltaTime);

        // Update cloth drop timers and release cloths when it's time to fall
        for (size_t i = 0; i < state.cloths.size(); i++) {
            if (!state.clothDropped[i]) {
                state.clothDropTimers[i] += deltaTime;

                // Calculate when this cloth should start falling
                float dropTime = state.clothDropStartDelay + (i * state.clothDropDelay);

                if (state.clothDropTimers[i] >= dropTime) {
                    state.clothDropped[i] = true;
                    
                    // Unpin particles for this cloth on GPU
                    size_t particleOffset = state.clothParticleOffsets[i];
                    state.physicsWorld.SetParticlesPinned(
                        particleOffset, state.clothParticleCounts[i], false);
                }
            }
        }

        // Update GPU physics simulation
        state.physicsWorld.Update(deltaTime);
    });

    // Set render callback
    app.SetRenderCallback([&]() {
        Render(state, app);
    });

    // Run application
    app.Run();

    return 0;
}
