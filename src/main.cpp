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

    // Create 3 cloths precisely centered to drape over the huge mirror sphere (radius 3.0)
    // Cloth 1 - Main drape
    ClothConfig clothConfig1;
    clothConfig1.widthSegments = 50;
    clothConfig1.heightSegments = 50;
    clothConfig1.segmentLength = 0.15f;
    clothConfig1.startX = -3.75f; // Centered
    clothConfig1.startY = 10.0f; // High enough to clear the sphere
    clothConfig1.startZ = -3.75f; // Centered
    clothConfig1.pinTopLeft = false;
    clothConfig1.pinTopRight = false;

    Cloth* cloth1 = new Cloth(state.physicsWorld, clothConfig1);
    ClothMesh* clothMesh1 = new ClothMesh(*cloth1);
    state.cloths.push_back(cloth1);
    state.clothMeshes.push_back(clothMesh1);

    // Cloth 2 - Offset and higher
    ClothConfig clothConfig2;
    clothConfig2.widthSegments = 40;
    clothConfig2.heightSegments = 40;
    clothConfig2.segmentLength = 0.15f;
    clothConfig2.startX = -3.0f;
    clothConfig2.startY = 12.0f; 
    clothConfig2.startZ = -3.0f;
    clothConfig2.pinTopLeft = false;
    clothConfig2.pinTopRight = false;

    Cloth* cloth2 = new Cloth(state.physicsWorld, clothConfig2);
    ClothMesh* clothMesh2 = new ClothMesh(*cloth2);
    state.cloths.push_back(cloth2);
    state.clothMeshes.push_back(clothMesh2);

    // Cloth 3 - Highest
    ClothConfig clothConfig3;
    clothConfig3.widthSegments = 40;
    clothConfig3.heightSegments = 40;
    clothConfig3.segmentLength = 0.15f;
    clothConfig3.startX = -3.0f;
    clothConfig3.startY = 14.0f; 
    clothConfig3.startZ = -3.0f;
    clothConfig3.pinTopLeft = false;
    clothConfig3.pinTopRight = false;

    Cloth* cloth3 = new Cloth(state.physicsWorld, clothConfig3);
    ClothMesh* clothMesh3 = new ClothMesh(*cloth3);
    state.cloths.push_back(cloth3);
    state.clothMeshes.push_back(clothMesh3);

    // Set update callback
    app.SetUpdateCallback([&](float deltaTime) {
        HandleInput(state, deltaTime);

        // Update physics
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
