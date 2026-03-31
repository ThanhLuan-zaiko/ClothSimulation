#include <iostream>
#include <filesystem>
#include <glm/glm.hpp>
#include <Windows.h>

#include "core/Application.h"
#include "AppState.h"
#include "InputHandler.h"
#include "SceneRenderer.h"
#include "utils/GPUDetection.h"
#include "utils/GPUBenchmark.h"
#include "utils/QualityClassifier.h"
#include "utils/ClothSimulationBenchmark.h"
#include "utils/AdaptiveQuality.h"

using namespace cloth;

// Global application pointer
static Application* g_App = nullptr;

// Global state pointer (defined here, used in Application.cpp)
namespace cloth {
    AppState* g_State = nullptr;
}

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
    // === APPLY TDR DELAY FIX FOR INTEL GPU ===
    // Increase GPU timeout from 2s to 10s to prevent driver timeout crashes
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
        "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers", 0, 
        KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD tdrDelay = 10;
        DWORD tdrDdiDelay = 10;
        RegSetValueExA(hKey, "TdrDelay", 0, REG_DWORD, (BYTE*)&tdrDelay, sizeof(tdrDelay));
        RegSetValueExA(hKey, "TdrDdiDelay", 0, REG_DWORD, (BYTE*)&tdrDdiDelay, sizeof(tdrDdiDelay));
        RegCloseKey(hKey);
        std::cout << "[GPU] TDR Delay applied: 10 seconds (RESTART REQUIRED for full effect)" << std::endl;
    } else {
        std::cout << "[GPU] Warning: Could not apply TDR delay (run as Administrator)" << std::endl;
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "     CLOTH SIMULATION - GPU AGNOSTIC" << std::endl;
    std::cout << "========================================\n" << std::endl;

    std::cout << "=== Cloth Simulation ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  WASD / Arrows         - Move camera (Horizontal)" << std::endl;
    std::cout << "  Q / Left Control      - Move camera Down / Up" << std::endl;
    std::cout << "  Shift + WASD          - Fast camera (Sprint)" << std::endl;
    std::cout << "  Mouse (Drag)          - Rotate camera (when not interacting)" << std::endl;
    std::cout << "  -----------------------------------------------------------" << std::endl;
    std::cout << "  E (Hold) + Mouse      - INTERACT with cloth (Vortex & Black Hole!)" << std::endl;
    std::cout << "  Space                 - Reset all cloths to initial drop positions" << std::endl;
    std::cout << "  H                     - Reset Camera to Home position" << std::endl;
    std::cout << "  -----------------------------------------------------------" << std::endl;
    std::cout << "  F                     - Toggle terrain wireframe" << std::endl;
    std::cout << "  T / Shift + T         - Change terrain texture" << std::endl;
    std::cout << "  1-4                   - Select terrain texture directly" << std::endl;
    std::cout << "  Escape                - Exit" << std::endl;
    std::cout << std::endl;
    std::cout << "Features:" << std::endl;
    std::cout << "  - Interactive Cloth: Use 'E' to swirl and pull fabric with your mouse." << std::endl;
    std::cout << "  - Visual Feedback: Green pulsing outline shows when you're in control." << std::endl;
    std::cout << "  - Mirror sphere at center reflects the environment" << std::endl;
    std::cout << "  - Mountain terrain with heightmap" << std::endl;
    std::cout << "  - 3 textured cloths draping over the sphere" << std::endl;
    std::cout << std::endl;
    std::cout << "Tip: Hold 'E' and move the mouse over the cloths to see the magic!" << std::endl;
    std::cout << std::endl;

    // Create application
    Application app;
    g_App = &app;

    // Create application state
    AppState state;
    g_State = &state;  // Set global state pointer

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

    // Load cloth shader from file
    state.clothShader = Shader::CreateFromFile(
        GetAssetPath("shaders/cloth/cloth.vert"),
        GetAssetPath("shaders/cloth/cloth.frag")
    );

    // Load SSAO shaders
    state.ssaoShader = Shader::CreateComputeShaderFromSource(Shader::LoadShaderSource(GetAssetPath("shaders/postprocess/ssao.comp")));
    state.ssaoBlurShader = Shader::CreateComputeShaderFromSource(Shader::LoadShaderSource(GetAssetPath("shaders/postprocess/ssao_blur.comp")));
    state.depthNormalShader = Shader::CreateFromFile(
        GetAssetPath("shaders/postprocess/depth_normal.vert"),
        GetAssetPath("shaders/postprocess/depth_normal.frag")
    );

    // Initialize SSAO buffer with window size
    int width, height;
    app.GetWindowSize(&width, &height);
    state.ssaoBuffer = new SSAOBuffer(width, height);

    // Initialize world (scan textures, create skybox)
    state.world.Initialize(GetAssetPath("assets/textures/lands/"));

    // Initialize world OpenGL resources (terrain mesh, etc.)
    state.world.InitializeGL();

    // Generate terrain heightmap AFTER OpenGL initialization
    state.world.GetTerrain().GenerateMountainHeightmap(3.0f);
    state.world.GetTerrain().SetHeightScale(1.0f);

    // Set terrain reference in GPU physics world for collision
    state.physicsWorld.SetTerrain(&state.world.GetTerrain());

    // Apply GPU-detected physics settings (from AppState)
    // Note: Quality level already set in AppState::InitializeGL() before physicsWorld.Initialize()
    GPUPhysicsConfig gpuPhysicsConfig;
    gpuPhysicsConfig.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    gpuPhysicsConfig.damping = 0.995f;
    gpuPhysicsConfig.iterations = state.gpuInfo.physicsIterations;  // LOW=2, MEDIUM=3, HIGH=4, ULTRA=5
    gpuPhysicsConfig.collisionMargin = 0.35f;  // INCREASED: Much larger margin for earlier collision detection
    gpuPhysicsConfig.dampingFactor = 0.85f;
    gpuPhysicsConfig.frictionFactor = 0.80f;
    gpuPhysicsConfig.collisionSubsteps = 32;   // INCREASED: More substeps for better collision detection
    gpuPhysicsConfig.ccdSubsteps = 32.0f;      // INCREASED: More CCD sub-intervals
    gpuPhysicsConfig.conservativeFactor = 2.0f; // INCREASED: More conservative advancement
    gpuPhysicsConfig.maxVelocity = 15.0f;      // LOWERED: More controlled motion
    gpuPhysicsConfig.sphereStaticFriction = 0.92f;
    gpuPhysicsConfig.sphereDynamicFriction = 0.20f;
    gpuPhysicsConfig.sphereWrapFactor = 0.80f;
    gpuPhysicsConfig.staticFrictionThreshold = 0.8f;
    gpuPhysicsConfig.sleepingThreshold = 0.30f;
    gpuPhysicsConfig.terrainDamping = 0.08f;
    
    // Update config (shader already compiled with correct iterations in InitializeGL)
    state.physicsWorld.SetConfig(gpuPhysicsConfig);
    state.physicsWorld.SetBatchCount(state.gpuInfo.batchCount);

    // Pre-load world textures
    state.world.LoadTextures();

    // Initialize reflection cubemap with sky color to avoid black flash
    if (state.reflectionCubemap && state.world.IsTexturesLoaded()) {
        state.reflectionCubemap->InitializeWithSkybox(state.world.GetSkybox().GetTextureID());
        // Force reflection update on first render
        state.reflectionNeedsUpdate = true;
    }

    // Update collision sphere position (ensure it matches the mirror sphere)
    state.physicsWorld.SetCollisionSphere(state.mirrorSphere.GetPosition(), state.mirrorSphere.GetRadius());

    // Create 3 cloths that will drop gracefully onto the mirror sphere
    // Each cloth starts high above and falls down with a delay
    // For GPU physics, we initialize cloth data directly on GPU

    // Cloth 1 - First to drop (CENTERED over sphere at (0,10,0))
    // Offset startX/startZ to center the cloth (cloth width = segments × segmentLength)
    ClothConfig clothConfig1;
    clothConfig1.widthSegments = state.gpuInfo.clothResolution[0];
    clothConfig1.heightSegments = state.gpuInfo.clothResolution[0];
    clothConfig1.segmentLength = 0.12f;
    
    // Calculate cloth dimensions
    float clothWidth = clothConfig1.widthSegments * clothConfig1.segmentLength;
    float clothHeight = clothConfig1.heightSegments * clothConfig1.segmentLength;
    
    // Center the cloth over sphere
    clothConfig1.startX = -clothWidth * 0.5f;   // Center X
    clothConfig1.startY = 18.0f;   // Above sphere (sphere is at y=10)
    clothConfig1.startZ = -clothHeight * 0.5f;   // Center Z

    size_t cloth1Offset = state.physicsWorld.InitializeCloth(
        clothConfig1.widthSegments, clothConfig1.heightSegments,
        clothConfig1.startX, clothConfig1.startY, clothConfig1.startZ,
        clothConfig1.segmentLength, true);  // Start pinned

    // Create ClothMesh for rendering (will read from GPU buffer)
    Cloth* dummyCloth1 = new Cloth(state.physicsWorld, clothConfig1);
    ClothMesh* clothMesh1 = new ClothMesh(*dummyCloth1);
    clothMesh1->SetGPUBased(true);
    clothMesh1->SetParticleBufferID(state.physicsWorld.GetPosBuffer());  // Updated to use buffer ID
    clothMesh1->SetParticleOffset(cloth1Offset); // Set the correct offset!
    state.cloths.push_back(dummyCloth1);
    state.clothMeshes.push_back(clothMesh1);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    size_t cloth1Count = (clothConfig1.widthSegments + 1) * (clothConfig1.heightSegments + 1);
    state.clothParticleCounts.push_back(cloth1Count);
    state.clothParticleOffsets.push_back(cloth1Offset);
    
    // Cloth 2
    ClothConfig clothConfig2;
    clothConfig2.widthSegments = state.gpuInfo.clothResolution[1];
    clothConfig2.heightSegments = state.gpuInfo.clothResolution[1];
    clothConfig2.segmentLength = 0.12f;
    clothConfig2.startX = -clothConfig2.widthSegments * clothConfig2.segmentLength * 0.5f;
    clothConfig2.startY = 22.0f;
    clothConfig2.startZ = -clothConfig2.heightSegments * clothConfig2.segmentLength * 0.5f;

    size_t cloth2Offset = state.physicsWorld.InitializeCloth(
        clothConfig2.widthSegments, clothConfig2.heightSegments,
        clothConfig2.startX, clothConfig2.startY, clothConfig2.startZ,
        clothConfig2.segmentLength, true);

    Cloth* dummyCloth2 = new Cloth(state.physicsWorld, clothConfig2);
    ClothMesh* clothMesh2 = new ClothMesh(*dummyCloth2);
    clothMesh2->SetGPUBased(true);
    clothMesh2->SetParticleBufferID(state.physicsWorld.GetPosBuffer());
    clothMesh2->SetParticleOffset(cloth2Offset); // Set the correct offset!
    state.cloths.push_back(dummyCloth2);
    state.clothMeshes.push_back(clothMesh2);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    state.clothParticleCounts.push_back((clothConfig2.widthSegments + 1) * (clothConfig2.heightSegments + 1));
    state.clothParticleOffsets.push_back(cloth2Offset);
    
    // Cloth 3
    ClothConfig clothConfig3;
    clothConfig3.widthSegments = state.gpuInfo.clothResolution[2];
    clothConfig3.heightSegments = state.gpuInfo.clothResolution[2];
    clothConfig3.segmentLength = 0.12f;
    clothConfig3.startX = -clothConfig3.widthSegments * clothConfig3.segmentLength * 0.5f;
    clothConfig3.startY = 26.0f;
    clothConfig3.startZ = -clothConfig3.heightSegments * clothConfig3.segmentLength * 0.5f;

    size_t cloth3Offset = state.physicsWorld.InitializeCloth(
        clothConfig3.widthSegments, clothConfig3.heightSegments,
        clothConfig3.startX, clothConfig3.startY, clothConfig3.startZ,
        clothConfig3.segmentLength, true);

    Cloth* dummyCloth3 = new Cloth(state.physicsWorld, clothConfig3);
    ClothMesh* clothMesh3 = new ClothMesh(*dummyCloth3);
    clothMesh3->SetGPUBased(true);
    clothMesh3->SetParticleBufferID(state.physicsWorld.GetPosBuffer());
    clothMesh3->SetParticleOffset(cloth3Offset); // Set the correct offset!
    state.cloths.push_back(dummyCloth3);
    state.clothMeshes.push_back(clothMesh3);
    state.clothDropTimers.push_back(0.0f);
    state.clothDropped.push_back(false);
    size_t cloth3Count = (clothConfig3.widthSegments + 1) * (clothConfig3.heightSegments + 1);
    state.clothParticleCounts.push_back(cloth3Count);
    state.clothParticleOffsets.push_back(cloth3Offset);  // Track offset

    // Update GPU VAOs for all cloths (buffer may have been resized)
    for (auto* mesh : state.clothMeshes) {
        mesh->UpdateGPUVAO();
    }

    // DO NOT re-set particle buffer references - already set correctly above!

    // Set update callback
    app.SetUpdateCallback([&](float deltaTime) {
        // Clamp deltaTime to avoid physics explosion on lag spikes
        if (deltaTime > 0.1f) {
            deltaTime = 0.1f;
        }

        state.totalTime += deltaTime;
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

        // Update GPU physics simulation (BATCH DISPATCH enabled)
        state.physicsWorld.Update(deltaTime);
    });

    // Set render callback
    app.SetRenderCallback([&]() {
        Render(state, app);
    });

    // Run application (window will show after first frame rendered)
    app.Run();

    return 0;
}
