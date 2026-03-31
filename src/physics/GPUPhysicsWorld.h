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

struct BendingConstraintDataType {
    glm::ivec4 indices; // p1, p2 (edge), p3, p4 (opposite vertices)
    glm::vec2 params;    // x = restAngle, y = stiffness
    glm::vec2 padding;   // x = anisotropic factor
};

struct GPUPhysicsConfig {
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);  // Standard gravity
    float damping = 0.995f;             // Air resistance
    int iterations = 16;                // Increased iterations for stiffer cloth
    float collisionMargin = 0.35f;      // INCREASED: Much larger margin for earlier collision detection (was 0.25)
    float dampingFactor = 0.85f;        // Higher = less energy loss on collision
    float frictionFactor = 0.80f;
    int collisionSubsteps = 32;         // INCREASED: More substeps for better collision detection (was 24)

    float gravityScale = 1.0f;          // Neutral gravity scale
    float airResistance = 0.996f;       // Higher = more air resistance
    float windStrength = 0.0f;
    glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.0f);

    // Advanced Cloth Attributes
    float structuralStiffness = 1.0f;     // Resistance to stretching
    float shearStiffness = 0.95f;         // Resistance to shearing
    float bendStiffness = 0.6f;           // Resistance to bending
    float skipStiffness = 0.8f;           // Resistance for long-range links

    float maxVelocity = 15.0f;            // LOWERED: More controlled motion (was 20)
    float selfCollisionRadius = 1.2f;     // Much larger for reliable collision detection
    float selfCollisionStrength = 0.95f;  // High strength for separation
    int collisionIterations = 12;

    // Sphere collision parameters
    float sphereStaticFriction = 0.92f;   // Very high = cloth catches on top
    float sphereDynamicFriction = 0.20f;  // Slightly higher for better sliding control
    float sphereBounce = 0.0f;            // No bounce
    float sphereGripFactor = 0.70f;       // High grip for draping
    float staticFrictionThreshold = 0.8f; // High = easy to stop and hang

    // NEW: Improved collision parameters
    float sleepingThreshold = 0.30f;      // Higher = more aggressive anti-jitter
    float sphereWrapFactor = 0.80f;       // Slightly lower to prevent over-sticking
    float terrainDamping = 0.08f;         // Low = soft terrain contact

    // CCD parameters - INCREASED for better collision detection
    float ccdSubsteps = 32.0f;            // INCREASED: More CCD sub-intervals for reliable detection (was 24)
    float conservativeFactor = 2.0f;      // INCREASED: More conservative advancement (was 1.8)
    float maxPenetrationCorrection = 0.5f; // Max correction per substep

    // Strain limiting parameters
    float strainLimitingStiffness = 0.8f; // Stiffness for strain correction
    float maxStrainRate = 0.05f;          // Maximum 5% strain per timestep
};

struct CollisionParams {
    glm::vec3 sphereCenter = glm::vec3(0.0f);
    float sphereRadius = 0.0f;
    float groundLevel = 0.0f;
    float collisionMargin = 0.0f;
    float dampingFactor = 0.0f;
    float frictionFactor = 0.0f;
    int collisionSubsteps = 0;
    float sphereStaticFriction = 0.0f;
    float sphereDynamicFriction = 0.0f;
    float sphereBounce = 0.0f;
    float sphereGripFactor = 0.0f;
    float staticFrictionThreshold = 0.0f;
    float sleepingThreshold = 0.0f;
    float sphereWrapFactor = 0.0f;
    float terrainDamping = 0.0f;
    float ccdSubsteps = 0.0f;
    float conservativeFactor = 0.0f;
    float maxPenetrationCorrection = 0.0f;
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
    Shader m_BendShader;
    Shader m_ApplyDeltasShader;
    Shader m_CollideShader;
    Shader m_CCDSolveShader;         // NEW: CCD with binary search TOI
    Shader m_SpatialHashShader;      // NEW: Spatial hash grid builder
    Shader m_ClearGridShader;
    Shader m_SweepPruneShader;
    Shader m_RadixSortHistShader;
    Shader m_RadixSortScanShader;
    Shader m_RadixSortScatterShader;
    Shader m_ResolveShader;
    Shader m_StrainLimitShader;

    bool m_ShadersLoaded;
    bool m_IsHighEndGPU;
    bool m_IsIntelGPU;

    // SoA Buffers (optimized bindings)
    unsigned int m_PosBuffer;              // binding 0
    unsigned int m_PrevPosBuffer;          // binding 1
    unsigned int m_ConstraintBuffer;       // binding 2
    unsigned int m_BendingConstraintBuffer;// binding 3
    unsigned int m_DeltaPositionBuffer;    // binding 4
    unsigned int m_PinnedFlagsBuffer;      // binding 5
    unsigned int m_ParticleDataBuffer;     // binding 6 (colors + adjacency combined)
    unsigned int m_ConstraintIndicesBuffer;// binding 7
    unsigned int m_GridHeadBuffer;         // binding 8
    unsigned int m_GridNextBuffer;         // binding 9
    unsigned int m_VelBuffer;              // binding 13 (optional)

    unsigned int m_UniformBuffer;          // UBO binding 0
    unsigned int m_CollisionUniformBuffer; // UBO binding 1

    // Sweep & Prune buffers
    unsigned int m_SortedIndicesBuffer;    // binding 10
    unsigned int m_SortTempBuffer;         // binding 11
    unsigned int m_CollisionPairsBuffer;   // binding 12

    // NEW: Spatial Hash Grid buffers
    unsigned int m_GridHeaderBuffer;       // binding 14
    unsigned int m_GridItemsBuffer;        // binding 15
    unsigned int m_PairCountBuffer;        // binding 16 (atomic counter)

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
    size_t m_TotalBendingConstraints;
    unsigned int m_ClothCount = 0;
    bool m_Initialized;
    std::vector<float> m_PinnedFlagsCPU;
};

} // namespace cloth
