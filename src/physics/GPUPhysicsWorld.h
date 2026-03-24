#pragma once

#include "ParticleBuffer.h"
#include "ConstraintBuffer.h"
#include "renderer/Shader.h"
#include "world/Terrain.h"
#include <glm/glm.hpp>
#include <vector>

namespace cloth {

// Maximum particles for all cloths
// Note: Each cloth with resolution N needs ~N*N particles and ~8*N*N constraints (with double bend/shear)
// For 3 cloths at 70x70: 3*5041=15123 particles, 3*49000=147000 constraints
const size_t MAX_PARTICLES = 20000;
const size_t MAX_CONSTRAINTS = 200000;  // Increased to support double constraints
const int MAX_COLORS = 9;  // Match shader (9-color pattern)

// GPU Physics simulation parameters
struct GPUPhysicsConfig {
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);  // Normal gravity
    float damping = 0.95f;               // Slightly more damping (was 0.98)
    int iterations = 8;                  // Higher default (was 5)
    float collisionMargin = 0.05f;
    float dampingFactor = 0.85f;
    float frictionFactor = 0.92f;
    int collisionSubsteps = 2;
    
    // NEW: Advanced physics parameters
    float gravityScale = 3.5f;           // Reduced (was 5.0)
    float airResistance = 0.992f;        // Reduced resistance (was 0.995)
    float windStrength = 0.0f;           // Wind strength (0 = no wind)
    glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.0f);  // Wind direction
    float stretchResistance = 0.8f;      // Increased (was 0.5)
    float maxVelocity = 35.0f;           // Reduced (was 40.0)
    float selfCollisionRadius = 0.08f;   // Self-collision detection radius
    float selfCollisionStrength = 0.35f; // Self-collision repulsion strength
    
    // Sphere friction parameters
    float sphereStaticFriction = 0.85f;    // Friction when nearly stopped
    float sphereDynamicFriction = 0.92f;   // Friction when sliding
    float sphereBounce = 0.10f;            // Reduced (was 0.15)
    float sphereGripFactor = 0.3f;         // Extra grip for cloth sticking
    float staticFrictionThreshold = 0.5f;  // Velocity threshold for static friction
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
    void SetConfig(const GPUPhysicsConfig& config);
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
    
    // Get particle buffer ID (for VAO setup)
    unsigned int GetParticleBufferID() const { return m_ParticleBuffer.GetBufferID(); }
    
    // Get pinned flags buffer ID
    unsigned int GetPinnedFlagsBufferID() const { return m_PinnedFlagsBuffer; }

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

    // Build constraint adjacency list for optimized constraint solving
    void BuildConstraintAdjacencyList();

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
    unsigned int m_PinnedFlagsBuffer;  // Dedicated buffer for pinned flags
    
    // Atomic collision buffers
    unsigned int m_CollisionCountBuffer;
    unsigned int m_CollisionDataBuffer;
    
    // Graph coloring buffers
    unsigned int m_ParticleColorsBuffer;
    unsigned int m_ColorCollisionCountBuffer;

    // Constraint adjacency list buffers (for optimized constraint solving)
    unsigned int m_ConstraintAdjacencyBuffer;  // (offset, count) for each particle
    unsigned int m_ConstraintIndicesBuffer;    // Flat array of constraint indices

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

    // CPU-side copy of pinned flags (to avoid GPU read-back issues)
    std::vector<float> m_PinnedFlagsCPU;
};

} // namespace cloth
