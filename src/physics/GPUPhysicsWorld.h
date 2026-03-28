#pragma once

#include "ParticleBuffer.h"
#include "ConstraintBuffer.h"
#include "renderer/Shader.h"
#include "world/Terrain.h"
#include <glm/glm.hpp>
#include <vector>

namespace cloth {

const size_t MAX_PARTICLES = 100000;
const size_t MAX_CONSTRAINTS = 1000000;
const int MAX_COLORS = 16;

struct ParticleDataType {
    glm::vec4 position;
    glm::vec4 prevPosition;
    glm::vec4 velocity;
    glm::vec4 padding;
};

struct ConstraintDataType {
    glm::ivec4 indices;
    glm::vec2 params;
    glm::vec2 padding;
};

struct GPUPhysicsConfig {
    glm::vec3 gravity = glm::vec3(0.0f, -5.0f, 0.0f);  // Reduced gravity for slower fall
    float damping = 0.990f;             // Slightly lower = more air resistance
    int iterations = 16;                // Increased iterations for stiffer cloth
    float collisionMargin = 0.03f;
    float dampingFactor = 0.92f;        // Higher = less energy loss on collision
    float frictionFactor = 0.90f;
    int collisionSubsteps = 10;

    float gravityScale = 1.0f;          // Neutral gravity scale
    float airResistance = 0.996f;       // Higher = more air resistance
    float windStrength = 0.0f;
    glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    
    // Advanced Cloth Attributes
    float structuralStiffness = 1.0f;     // Resistance to stretching
    float shearStiffness = 0.95f;         // Resistance to shearing
    float bendStiffness = 0.6f;           // Resistance to bending
    float skipStiffness = 0.8f;           // Resistance for long-range links
    
    float maxVelocity = 25.0f;            // Lower max velocity for more controlled motion
    float selfCollisionRadius = 1.2f;     // Much larger for reliable collision detection
    float selfCollisionStrength = 0.95f;  // High strength for separation
    int collisionIterations = 12;

    // Sphere collision parameters
    float sphereStaticFriction = 0.98f;   // Very high = cloth catches on top
    float sphereDynamicFriction = 0.10f;  // Very low = minimal reduction from max friction
    float sphereBounce = 0.0f;            // No bounce
    float sphereGripFactor = 0.8f;        // High grip for draping
    float staticFrictionThreshold = 1.2f; // Very high = easy to stop and hang
    
    // NEW: Improved collision parameters
    float sleepingThreshold = 0.25f;      // Higher = more aggressive anti-jitter
    float sphereWrapFactor = 0.95f;       // Very high = wraps a lot
    float terrainDamping = 0.02f;         // Very low = soft terrain contact
};

struct CollisionParams {
    glm::vec3 sphereCenter = glm::vec3(0.0f);
    float sphereRadius = 0.0f;
    float groundLevel = 0.0f;
    bool hasSphere = false;
};

class GPUPhysicsWorld {
public:
    GPUPhysicsWorld();
    ~GPUPhysicsWorld();

    bool Initialize(const GPUPhysicsConfig& config = GPUPhysicsConfig());
    size_t InitializeCloth(int widthSegments, int heightSegments, float startX, float startY, float startZ, float segmentLength, bool pinned);
    void ResetCloth(size_t offset, int widthSegments, int heightSegments, float startX, float startY, float startZ, float segmentLength, bool pinned, int clothID);
    void SetParticlesPinned(size_t startIdx, size_t count, bool pinned);
    void Update(float deltaTime);
    void Unbind() const;
    void SetBatchCount(unsigned int batchCount) { m_BatchSize = batchCount; }

    unsigned int GetPosBuffer() const { return m_PosBuffer; }
    unsigned int GetPrevPosBuffer() const { return m_PrevPosBuffer; }
    unsigned int GetVelBuffer() const { return m_VelBuffer; }

    void SetConfig(const GPUPhysicsConfig& config);
    const GPUPhysicsConfig& GetConfig() const { return m_Config; }
    void SetCollisionSphere(const glm::vec3& center, float radius);
    void SetGroundLevel(float y) { m_Collision.groundLevel = y; UpdateUniforms(1.0f / 60.0f); }
    void SetTerrain(const Terrain* terrain) { m_Terrain = terrain; CreateTerrainHeightmapTexture(); }
    void AutoTuneSettings();
    void SetQualityLevel(int iterations, bool useTextureGather);
    int GetQualityLevel() const { return m_MaxIterations; }
    void SetManualPreset(bool manual) { m_ManualPresetSelected = manual; m_AutoTuningEnabled = !manual; }
    size_t GetTotalParticles() const { return m_TotalParticles; }
    bool IsInitialized() const { return m_Initialized; }
    unsigned int GetParticleBufferID() const { return m_PosBuffer; }

private:
    bool LoadComputeShaders();
    void UpdateUniforms(float deltaTime);
    void CreateTerrainHeightmapTexture();
    void BuildConstraintAdjacencyList();

    Shader m_IntegrateShader;
    Shader m_SolveShader;
    Shader m_CollideShader;
    Shader m_ClearGridShader;
    Shader m_SweepPruneShader;
    Shader m_RadixSortHistShader;
    Shader m_RadixSortScanShader;
    Shader m_RadixSortScatterShader;
    Shader m_ResolveShader;

    bool m_ShadersLoaded;
    bool m_IsHighEndGPU;
    bool m_IsIntelGPU;

    // SoA Buffers
    unsigned int m_PosBuffer;
    unsigned int m_PrevPosBuffer;
    unsigned int m_VelBuffer;
    unsigned int m_ConstraintBuffer;
    unsigned int m_ParticleColorsBuffer;

    unsigned int m_PinnedFlagsBuffer;
    unsigned int m_ConstraintAdjacencyBuffer;
    unsigned int m_ConstraintIndicesBuffer;
    unsigned int m_GridHeadBuffer;
    unsigned int m_GridNextBuffer;
    unsigned int m_UniformBuffer;
    unsigned int m_CollisionUniformBuffer;
    
    // Sweep & Prune buffers
    unsigned int m_SortedIndicesBuffer;
    unsigned int m_SortTempBuffer;
    unsigned int m_CollisionPairsBuffer;

    GPUPhysicsConfig m_Config;
    CollisionParams m_Collision;
    const Terrain* m_Terrain;
    unsigned int m_TerrainHeightmapTexture;

    unsigned int m_WorkGroupSize;
    unsigned int m_BatchSize;
    int m_MaxIterations;
    bool m_UseTextureGather;
    bool m_AutoTuningEnabled;
    bool m_ManualPresetSelected;
    size_t m_TotalParticles;
    size_t m_TotalConstraints;
    unsigned int m_ClothCount = 0;
    bool m_Initialized;
    std::vector<float> m_PinnedFlagsCPU;
};

} // namespace cloth
