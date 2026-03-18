#pragma once

#include "ParticleBuffer.h"
#include "ConstraintBuffer.h"
#include "renderer/Shader.h"
#include "world/Terrain.h"
#include <glm/glm.hpp>
#include <vector>

namespace cloth {

// GPU Physics simulation parameters
struct GPUPhysicsConfig {
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float damping = 0.98f;
    int iterations = 5;
    float collisionMargin = 0.05f;
    float dampingFactor = 0.85f;
    float frictionFactor = 0.92f;
    int collisionSubsteps = 2;  // Number of substeps for collision detection
};

// Collision parameters
struct CollisionParams {
    glm::vec3 sphereCenter = glm::vec3(0.0f);
    float sphereRadius = 0.0f;
    float groundLevel = 0.0f;
    bool hasSphere = false;
};

/**
 * GPU-accelerated physics world using Compute Shader
 * Handles particle simulation, constraints, and collisions entirely on GPU
 */
class GPUPhysicsWorld {
public:
    GPUPhysicsWorld();
    ~GPUPhysicsWorld();

    // Initialize compute shader and buffers
    bool Initialize(const GPUPhysicsConfig& config = GPUPhysicsConfig());

    // Initialize buffers for a cloth with given dimensions
    // Returns the particle offset for this cloth
    size_t InitializeCloth(int widthSegments, int heightSegments,
                           float startX, float startY, float startZ,
                           float segmentLength, bool pinned);

    // Set pinned state for a range of particles (for cloth drop animation)
    void SetParticlesPinned(size_t startIdx, size_t count, bool pinned);

    // Update physics simulation
    void Update(float deltaTime);

    // Bind all SSBOs for rendering
    void BindForRendering() const;

    // Unbind all SSBOs
    void Unbind() const;

    // Get particle buffer for accessing data
    const ParticleBuffer& GetParticleBuffer() const { return m_ParticleBuffer; }
    ParticleBuffer& GetParticleBuffer() { return m_ParticleBuffer; }

    // Get constraint buffer
    const ConstraintBuffer& GetConstraintBuffer() const { return m_ConstraintBuffer; }
    ConstraintBuffer& GetConstraintBuffer() { return m_ConstraintBuffer; }

    // Configuration
    void SetConfig(const GPUPhysicsConfig& config) { m_Config = config; UpdateUniforms(1.0f / 60.0f); }
    const GPUPhysicsConfig& GetConfig() const { return m_Config; }
    
    // Set batch count for GPU dispatch (for TDR prevention on Intel GPU)
    void SetBatchCount(unsigned int batchCount) { m_BatchSize = batchCount; }
    unsigned int GetBatchCount() const { return m_BatchSize; }

    // Set work group size (for dynamic GPU-specific optimization)
    void SetWorkGroupSize(unsigned int size) { m_WorkGroupSize = size; }
    unsigned int GetWorkGroupSize() const { return m_WorkGroupSize; }

    // Collision setup
    void SetCollisionSphere(const glm::vec3& center, float radius);
    void SetGroundLevel(float y) { m_Collision.groundLevel = y; UpdateUniforms(1.0f / 60.0f); }
    void SetTerrain(const Terrain* terrain) { m_Terrain = terrain; CreateTerrainHeightmapTexture(); }

    // Auto-tune and adaptive quality
    void AutoTuneSettings();  // Run benchmark and auto-detect optimal settings
    void SetQualityLevel(int iterations, bool useTextureGather);
    int GetQualityLevel() const { return m_MaxIterations; }
    
    // Manual preset override
    void SetManualPreset(bool manual) { m_ManualPresetSelected = manual; m_AutoTuningEnabled = !manual; }
    bool IsManualPreset() const { return m_ManualPresetSelected; }
    
    // Get total particle/constraint count
    size_t GetTotalParticles() const { return m_TotalParticles; }
    size_t GetTotalConstraints() const { return m_TotalConstraints; }

    // Check if initialized
    bool IsInitialized() const { return m_Initialized; }

    // Download particle data (for debugging)
    std::vector<ParticleData> DownloadParticles() const;

private:
    // Load and compile compute shader
    bool LoadComputeShader();
    
    // Helper function to replace strings in shader source
    std::string replace(const std::string& str, const std::string& from, const std::string& to);

    // Update uniform buffers
    void UpdateUniforms(float deltaTime);

    // Create terrain heightmap texture for GPU access
    void CreateTerrainHeightmapTexture();

    // Compute shader
    Shader m_ComputeShader;
    bool m_ShaderLoaded;
    bool m_ShaderValidated;  // Track if shader has been validated

    // GPU detection
    bool m_IsHighEndGPU;     // NVIDIA or AMD
    bool m_IsIntelGPU;       // Intel integrated

    // Buffers
    ParticleBuffer m_ParticleBuffer;
    ConstraintBuffer m_ConstraintBuffer;

    // Uniform buffer object
    unsigned int m_UniformBuffer;
    unsigned int m_CollisionUniformBuffer;

    // Configuration
    GPUPhysicsConfig m_Config;
    CollisionParams m_Collision;

    // Terrain
    const Terrain* m_Terrain;
    unsigned int m_TerrainHeightmapTexture;

    // Work group size (from compute shader)
    unsigned int m_WorkGroupSize;

    // Batch dispatch for Intel GPU (prevent TDR timeout)
    static const unsigned int NUM_BATCHES = 4;  // Split dispatch into 4 batches
    unsigned int m_BatchSize;

    // Uniform block indices (cached after initialization)
    int m_PhysicsParamsBlockIndex;
    int m_CollisionParamsBlockIndex;

    // Auto-tune settings
    int m_MaxIterations;           // Dynamic based on GPU performance
    bool m_UseTextureGather;       // Dynamic based on GPU type
    bool m_AutoTuningEnabled;      // If true, auto-adjust quality at runtime
    bool m_ManualPresetSelected;   // If true, user manually selected preset (don't auto-tune)
    
    // Tracking
    size_t m_TotalParticles;
    size_t m_TotalConstraints;
    bool m_Initialized;
};

} // namespace cloth
