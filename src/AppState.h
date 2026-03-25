#pragma once

#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "renderer/Camera.h"
#include "renderer/Shader.h"
#include "renderer/ReflectionCubemap.h"
#include "physics/GPUPhysicsWorld.h"
#include "cloth/Cloth.h"
#include "cloth/ClothMesh.h"
#include "world/World.h"
#include "world/Sphere.h"
#include "utils/GPUDetection.h"

using namespace cloth;

namespace cloth {

// Global application state
struct AppState {
    // GPU info (detected after OpenGL context creation)
    GPUInfo gpuInfo;
    bool gpuDetected;
    QualityPreset selectedPreset;
    
    // Adaptive quality system
    AdaptiveQuality adaptiveQuality;
    
    // Camera
    Camera camera;
    float cameraYaw = -90.0f;
    float cameraPitch = -20.0f;
    float cameraFOV = 45.0f;

    // Physics (GPU-based)
    GPUPhysicsWorld physicsWorld;

    // Cloths
    std::vector<Cloth*> cloths;
    std::vector<ClothMesh*> clothMeshes;
    std::vector<unsigned int> clothTextures;
    std::vector<size_t> clothParticleCounts;      // Number of particles per cloth (for GPU physics)
    std::vector<size_t> clothParticleOffsets;     // Cumulative particle offsets for each cloth (for GPU unpinning)
    
    // Cloth drop animation
    std::vector<float> clothDropTimers;      // Timer for each cloth drop
    std::vector<bool> clothDropped;          // Whether each cloth has started falling
    float clothDropDelay = 3.0f;             // Delay between each cloth drop (increased for dramatic effect)
    float clothDropStartDelay = 1.0f;        // Initial delay before first cloth drops

    // World and sphere
    World world;
    Sphere mirrorSphere;

    // Shaders
    Shader clothShader;
    Shader terrainShader;
    Shader sphereShader;
    Shader skyboxShader;

    // Reflection
    ReflectionCubemap* reflectionCubemap = nullptr;  // Use pointer - initialized in InitializeGL()
    bool reflectionNeedsUpdate = true;

    // Lazy update tracking
    glm::vec3 lastReflectionUpdatePos;  // Last camera position when reflection was updated
    glm::vec3 lastReflectionViewDir;    // Last view direction when reflection was updated
    float reflectionUpdateThreshold = 3.0f;  // Distance threshold for updating reflection
    int lastTerrainTextureIndex = -1;  // Track terrain texture index
    
    // Reflection quality settings (configurable)
    int reflectionResolution = 1024;     // 256, 512, 1024
    bool reflectionHDR = true;          // Enable HDR rendering
    float reflectionExposure = 1.2f;    // HDR exposure value
    float sphereMetallic = 1.0f;        // PBR metallic (0-1)
    float sphereRoughness = 0.05f;      // PBR roughness (0-1, lower = smoother)
    bool reflectionLOD = true;          // Enable LOD bias for performance

    // Terrain texture state
    unsigned int terrainTextureID = 0;
    int terrainVAO = 0;
    int terrainIndexCount = 0;
    int currentTerrainTextureIndex = -1;
    bool terrainTextureLoaded = false;

    // Cloth textures state
    bool clothTexturesLoaded = false;

    // Mouse state
    bool mousePressed = false;
    double lastMouseX = 0;
    double lastMouseY = 0;

    // Key repeat tracking
    float keyRepeatTimer = 0.0f;
    float keyRepeatDelay = 0.15f;
    bool lastTKeyPressed = false;
    bool lastShiftKeyPressed = false;
    bool lastFKeyPressed = false;
    bool lastNumberKeysPressed[9] = {false};

    // Constructor
    AppState()
        : camera(ProjectionType::Perspective)
        , gpuDetected(false)
        , selectedPreset(QualityPreset::LOW)
    {
        // Initialize camera position - looking at the scene from a distance
        camera.SetPosition(glm::vec3(0.0f, 15.0f, 25.0f));
        camera.SetRotation(glm::vec3(-90.0f, -20.0f, 0.0f));

        // GPU Physics world will be initialized in InitializeGL() after GL context is ready
        // Cloth shader will be loaded from file in main.cpp
    }

    // Initialize OpenGL-dependent objects (call after GL context is ready)
    void InitializeGL() {
        // === GPU DETECTION (must be after GL context creation) ===
        std::cout << "\n[GPU] Detecting GPU..." << std::endl;
        gpuInfo = GPUDetector::Detect();
        gpuDetected = true;
        
        std::cout << "\n=== GPU INFORMATION ===" << std::endl;
        std::cout << "  Name: " << gpuInfo.name << std::endl;
        std::cout << "  Vendor: " << gpuInfo.vendor << std::endl;
        std::cout << "  OpenGL: " << gpuInfo.glVersion << std::endl;
        std::cout << "  Detected Preset: " << GPUDetector::GetPresetName(gpuInfo.detectedPreset) << std::endl;
        std::cout << "\n=== GPU CAPABILITIES ===" << std::endl;
        std::cout << "  Compute Shader: " << (gpuInfo.supportsComputeShader ? "YES" : "NO") << std::endl;
        std::cout << "  SSBO: " << (gpuInfo.supportsSSBO ? "YES" : "NO") << std::endl;
        std::cout << "  Persistent Mapping: " << (gpuInfo.supportsPersistentMapping ? "YES" : "NO") << std::endl;
        std::cout << "\n=== PHYSICS SETTINGS ===" << std::endl;
        std::cout << "  Iterations: " << gpuInfo.physicsIterations << std::endl;
        std::cout << "  Collision Substeps: " << gpuInfo.collisionSubsteps << std::endl;
        std::cout << "  Work Group Size: " << gpuInfo.workGroupSize << std::endl;
        std::cout << "  Batch Count: " << gpuInfo.batchCount << std::endl;
        std::cout << "\n=== CLOTH RESOLUTION ===" << std::endl;
        std::cout << "  Cloth 1: " << gpuInfo.clothResolution[0] << "x" << gpuInfo.clothResolution[0] << std::endl;
        std::cout << "  Cloth 2: " << gpuInfo.clothResolution[1] << "x" << gpuInfo.clothResolution[1] << std::endl;
        std::cout << "  Cloth 3: " << gpuInfo.clothResolution[2] << "x" << gpuInfo.clothResolution[2] << std::endl;

        // === RUN BENCHMARK ===
        GPUDetector::RunBenchmark(gpuInfo);

        // === SHOW QUALITY MENU ===
        selectedPreset = GPUDetector::ShowQualityMenu(gpuInfo);

        // Apply selected preset
        GPUDetector::ApplyPresetSettings(gpuInfo, selectedPreset);

        std::cout << "\n[GPU] Using preset: " << GPUDetector::GetPresetName(selectedPreset) << std::endl;
        std::cout << "[GPU] Starting simulation...\n" << std::endl;

        // Mark as manual preset selection (disable auto-tune)
        physicsWorld.SetManualPreset(true);

        // === INITIALIZE ADAPTIVE QUALITY ===
        adaptiveQuality.Initialize(selectedPreset);
        adaptiveQuality.SetEnabled(true);
        std::cout << "[AdaptiveQuality] Enabled - will auto-adjust to maintain 60 FPS" << std::endl;

        // Gigantic 3m radius mirror sphere
        mirrorSphere.SetRadius(3.0f);
        mirrorSphere.Initialize();
        mirrorSphere.SetPosition(glm::vec3(0.0f, 10.0f, 0.0f)); // Raised above ground for better visibility
        mirrorSphere.SetColor(glm::vec3(1.0f, 1.0f, 1.0f));

        // Apply reflection settings from GPU preset
        reflectionResolution = gpuInfo.reflectionResolution;
        reflectionHDR = gpuInfo.reflectionHDR;
        reflectionLOD = gpuInfo.reflectionLOD;
        reflectionExposure = gpuInfo.reflectionExposure;

        // Initialize reflection cubemap (AFTER GL context is ready!)
        // Resolution and HDR based on GPU preset
        reflectionCubemap = new ReflectionCubemap(reflectionResolution, reflectionHDR);

        std::cout << "[Reflection] Resolution: " << reflectionResolution << "x" << reflectionResolution
                  << ", HDR: " << (reflectionHDR ? "ON" : "OFF")
                  << ", LOD: " << (reflectionLOD ? "ON" : "OFF")
                  << ", Exposure: " << reflectionExposure << std::endl;

        // Initialize lazy update tracking
        lastReflectionUpdatePos = camera.GetPosition();
        lastReflectionViewDir = camera.GetFront();
        lastTerrainTextureIndex = -1;

        // Initialize GPU physics world with GPU-detected settings
        GPUPhysicsConfig config;
        config.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        config.damping = 0.99f;
        config.iterations = gpuInfo.physicsIterations;
        config.collisionMargin = 0.05f;
        config.dampingFactor = 0.95f;
        config.frictionFactor = 0.98f;
        config.collisionSubsteps = gpuInfo.collisionSubsteps;
        
        // INTER-CLOTH COLLISION SETTINGS (More robust)
        config.selfCollisionRadius = 0.35f;   // Increased cushion (35cm)
        config.selfCollisionStrength = 2.0f;  // Stronger repulsion
        
        // STABILITY SETTINGS
        config.gravityScale = 3.0f;           // Slightly reduced for better stability
        config.airResistance = 0.990f;        // Slightly more resistance to prevent over-acceleration
        
        // CRITICAL: Set quality level BEFORE Initialize() so shader compiles with correct iterations
        bool useTextureGather = (selectedPreset == QualityPreset::HIGH || 
                                  selectedPreset == QualityPreset::ULTRA);
        physicsWorld.SetQualityLevel(gpuInfo.physicsIterations, useTextureGather);
        
        // Now initialize with matching config
        physicsWorld.Initialize(config);

        // Set collision sphere in physics world
        physicsWorld.SetCollisionSphere(mirrorSphere.GetPosition(), mirrorSphere.GetRadius());
    }

    // Destructor - clean up dynamically allocated resources
    ~AppState() {
        // Clean up reflection cubemap
        if (reflectionCubemap) {
            delete reflectionCubemap;
            reflectionCubemap = nullptr;
        }
        
        // Clean up cloth textures
        for (unsigned int texID : clothTextures) {
            if (glIsTexture(texID)) {
                glDeleteTextures(1, &texID);
            }
        }

        // Clean up terrain texture
        if (terrainTextureLoaded && glIsTexture(terrainTextureID)) {
            glDeleteTextures(1, &terrainTextureID);
        }

        // Clean up cloths and cloth meshes
        for (auto* mesh : clothMeshes) {
            delete mesh;
        }
        for (auto* c : cloths) {
            delete c;
        }
    }
};

} // namespace cloth
