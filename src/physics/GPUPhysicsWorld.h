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
const int MAX_COLORS = 8;

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
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);
    float damping = 0.98f;                // Velocity damping (higher = less air resistance)
    int iterations = 12;                  // Constraint iterations (more = stiffer cloth)
    float collisionMargin = 0.03f;        // Collision skin thickness
    float dampingFactor = 0.90f;          // Energy loss on collision
    float frictionFactor = 0.95f;         // Ground friction coefficient
    int collisionSubsteps = 8;            // Physics substeps for collision

    float gravityScale = 4.0f;            // Gravity multiplier for Verlet
    float airResistance = 0.995f;         // Velocity damping per frame
    float windStrength = 0.0f;            // Wind force magnitude
    glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.0f);
    float stretchResistance = 0.9f;       // Resistance to stretching
    float maxVelocity = 50.0f;            // Max particle velocity (m/s)
    float selfCollisionRadius = 0.30f;    // Particle collision radius
    float selfCollisionStrength = 1.5f;   // Collision repulsion strength

    // Sphere collision parameters
    float sphereStaticFriction = 0.92f;   // Static friction (when at rest)
    float sphereDynamicFriction = 0.88f;  // Dynamic friction (when moving)
    float sphereBounce = 0.05f;           // Restitution coefficient (0 = no bounce)
    float sphereGripFactor = 0.5f;        // Surface grip/adhesion
    float staticFrictionThreshold = 0.8f; // Velocity threshold for static friction
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
