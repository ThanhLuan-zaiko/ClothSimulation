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
    
    // Set terrain reference in physics world for collision
    state.physicsWorld.SetTerrain(&state.world.GetTerrain());

    // Create 3 cloths that will drop gracefully onto the mirror sphere
    // Each cloth starts high above and falls down with a delay
    
    // Cloth 1 - First to drop (starts at y=25)
    ClothConfig clothConfig1;
    clothConfig1.widthSegments = 60;
    clothConfig1.heightSegments = 60;
    clothConfig1.segmentLength = 0.12f;
    clothConfig1.startX = -3.6f;
    clothConfig1.startY = 25.0f; // High above the sphere
    clothConfig1.startZ = -3.6f;
    clothConfig1.pinTopLeft = false;
    clothConfig1.pinTopRight = false;

    Cloth* cloth1 = new Cloth(state.physicsWorld, clothConfig1);
    ClothMesh* clothMesh1 = new ClothMesh(*cloth1);
    state.cloths.push_back(cloth1);
    state.clothMeshes.push_back(clothMesh1);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    cloth1->SetAllParticlesPinned(true); // Pin all particles to hold in place

    // Cloth 2 - Second to drop (starts at y=28, offset)
    ClothConfig clothConfig2;
    clothConfig2.widthSegments = 55;
    clothConfig2.heightSegments = 55;
    clothConfig2.segmentLength = 0.12f;
    clothConfig2.startX = -3.3f;
    clothConfig2.startY = 28.0f; // Higher than cloth 1
    clothConfig2.startZ = -3.3f;
    clothConfig2.pinTopLeft = false;
    clothConfig2.pinTopRight = false;

    Cloth* cloth2 = new Cloth(state.physicsWorld, clothConfig2);
    ClothMesh* clothMesh2 = new ClothMesh(*cloth2);
    state.cloths.push_back(cloth2);
    state.clothMeshes.push_back(clothMesh2);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    cloth2->SetAllParticlesPinned(true); // Pin all particles to hold in place

    // Cloth 3 - Third to drop (starts at y=31, offset)
    ClothConfig clothConfig3;
    clothConfig3.widthSegments = 55;
    clothConfig3.heightSegments = 55;
    clothConfig3.segmentLength = 0.12f;
    clothConfig3.startX = -3.3f;
    clothConfig3.startY = 31.0f; // Highest
    clothConfig3.startZ = -3.3f;
    clothConfig3.pinTopLeft = false;
    clothConfig3.pinTopRight = false;

    Cloth* cloth3 = new Cloth(state.physicsWorld, clothConfig3);
    ClothMesh* clothMesh3 = new ClothMesh(*cloth3);
    state.cloths.push_back(cloth3);
    state.clothMeshes.push_back(clothMesh3);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    cloth3->SetAllParticlesPinned(true); // Pin all particles to hold in place

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
                    state.cloths[i]->SetAllParticlesPinned(false); // Release!
                }
            }
        }

        // Update physics for all particles
        state.physicsWorld.Update(deltaTime);

        // Update all cloth meshes
        for (auto* clothMesh : state.clothMeshes) {
            clothMesh->Update();
        }
    });

    // Set render callback
    app.SetRenderCallback([&]() {
        Render(state, app);
    });

    // Run application
    app.Run();

    return 0;
}
