#include "GPUPhysicsWorld.h"
#include "utils/GPUDetection.h"
#include "cloth/Cloth.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace cloth {

GPUPhysicsWorld::GPUPhysicsWorld()
    : m_ShadersLoaded(false),
      m_IsHighEndGPU(false),
      m_IsIntelGPU(false),
      m_Terrain(nullptr),
      m_TerrainHeightmapTexture(0),
      m_PosBuffer(0),
      m_PrevPosBuffer(0),
      m_VelBuffer(0),
      m_ConstraintBuffer(0),
      m_ParticleColorsBuffer(0),
      m_PinnedFlagsBuffer(0),
      m_ConstraintAdjacencyBuffer(0),
      m_ConstraintIndicesBuffer(0),
      m_GridHeadBuffer(0),
      m_GridNextBuffer(0),
      m_UniformBuffer(0),
      m_CollisionUniformBuffer(0),
      m_SortedIndicesBuffer(0),
      m_SortTempBuffer(0),
      m_CollisionPairsBuffer(0),
      m_WorkGroupSize(128),
      m_BatchSize(4),
      m_MaxIterations(8),
      m_UseTextureGather(false),
      m_AutoTuningEnabled(true),
      m_ManualPresetSelected(false),
      m_TotalParticles(0),
      m_TotalConstraints(0),
      m_Initialized(false) {}

GPUPhysicsWorld::~GPUPhysicsWorld() {
    if (m_UniformBuffer) glDeleteBuffers(1, &m_UniformBuffer);
    if (m_CollisionUniformBuffer) glDeleteBuffers(1, &m_CollisionUniformBuffer);
    if (m_PosBuffer) glDeleteBuffers(1, &m_PosBuffer);
    if (m_PrevPosBuffer) glDeleteBuffers(1, &m_PrevPosBuffer);
    if (m_VelBuffer) glDeleteBuffers(1, &m_VelBuffer);
    if (m_ConstraintBuffer) glDeleteBuffers(1, &m_ConstraintBuffer);
    if (m_ParticleColorsBuffer) glDeleteBuffers(1, &m_ParticleColorsBuffer);
    if (m_PinnedFlagsBuffer) glDeleteBuffers(1, &m_PinnedFlagsBuffer);
    if (m_ConstraintAdjacencyBuffer) glDeleteBuffers(1, &m_ConstraintAdjacencyBuffer);
    if (m_ConstraintIndicesBuffer) glDeleteBuffers(1, &m_ConstraintIndicesBuffer);
    if (m_GridHeadBuffer) glDeleteBuffers(1, &m_GridHeadBuffer);
    if (m_GridNextBuffer) glDeleteBuffers(1, &m_GridNextBuffer);
    if (m_TerrainHeightmapTexture) glDeleteTextures(1, &m_TerrainHeightmapTexture);
}

bool GPUPhysicsWorld::Initialize(const GPUPhysicsConfig& config) {
    m_Config = config;
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    m_IsHighEndGPU = (vendor && (strstr(vendor, "NVIDIA") || strstr(vendor, "AMD")));

    // Check OpenGL version (need 4.3+ for compute shaders)
    int major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    
    if (major < 4 || (major == 4 && minor < 3)) {
        std::cerr << "[Physics] ERROR: OpenGL 4.3+ required for compute shaders!" << std::endl;
        return false;
    }

    if (!LoadComputeShaders()) return false;

    // Allocate Particle Buffers
    glGenBuffers(1, &m_PosBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PosBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_PrevPosBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PrevPosBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_VelBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_VelBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);

    // Allocate Constraint Buffers
    glGenBuffers(1, &m_ConstraintBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_CONSTRAINTS * sizeof(ConstraintDataType), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_PinnedFlagsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_ParticleColorsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleColorsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
    
    glGenBuffers(1, &m_ConstraintAdjacencyBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintAdjacencyBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(glm::ivec2), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_ConstraintIndicesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintIndicesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_CONSTRAINTS * 2 * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

    // Grid Buffers - match shader GRID_SIZE
    const unsigned int GRID_SIZE = 32768u;
    std::vector<unsigned int> initGrid(GRID_SIZE, 0xFFFFFFFF);
    glGenBuffers(1, &m_GridHeadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GridHeadBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, GRID_SIZE * sizeof(unsigned int), initGrid.data(), GL_DYNAMIC_DRAW);

    std::vector<unsigned int> initNext(MAX_PARTICLES, 0xFFFFFFFF);
    glGenBuffers(1, &m_GridNextBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GridNextBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(unsigned int), initNext.data(), GL_DYNAMIC_DRAW);

    // Uniform Buffers
    glGenBuffers(1, &m_UniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 160, nullptr, GL_DYNAMIC_DRAW); // Updated size to match struct
    
    glGenBuffers(1, &m_CollisionUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_CollisionUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 128, nullptr, GL_DYNAMIC_DRAW); 

    // New Radix Sort / Sweep & Prune Buffers
    glGenBuffers(1, &m_SortedIndicesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SortedIndicesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
    
    glGenBuffers(1, &m_SortTempBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SortTempBuffer);
    // Temp buffer: sorted indices + histograms + pair count (needs 2 * MAX_PARTICLES)
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * 2 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
    
    const unsigned int MAX_PAIRS = 524288u;
    glGenBuffers(1, &m_CollisionPairsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CollisionPairsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PAIRS * sizeof(unsigned int) * 2, nullptr, GL_DYNAMIC_DRAW);

    UpdateUniforms(1.0f / 60.0f);
    m_Initialized = true;
    return true;
}

bool GPUPhysicsWorld::LoadComputeShaders() {
    auto preprocess = [](const std::string& path) -> std::string {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Physics] Error: Could not open shader file: " << path << std::endl;
            return "";
        }
        std::stringstream ss;
        std::string line;
        while (std::getline(file, line)) {
            if (line.find("#include") != std::string::npos) {
                size_t start = line.find('"');
                size_t end = line.find('"', start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    std::string includePath = "shaders/physics/modular/" + line.substr(start + 1, end - start - 1);
                    std::ifstream inc(includePath);
                    if (!inc.is_open()) {
                        std::cerr << "[Physics] Error: Could not open included shader: " << includePath << std::endl;
                    } else {
                        ss << inc.rdbuf() << "\n";
                    }
                }
            } else {
                ss << line << "\n";
            }
        }
        return ss.str();
    };

    m_IntegrateShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_integrate.comp"));
    if (!m_IntegrateShader.GetID()) return false;

    m_SolveShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_solve.comp"));
    if (!m_SolveShader.GetID()) return false;

    m_RadixSortHistShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_radix_sort_hist.comp"));
    if (!m_RadixSortHistShader.GetID()) return false;

    m_RadixSortScanShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_radix_sort_scan.comp"));
    if (!m_RadixSortScanShader.GetID()) return false;

    m_RadixSortScatterShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_radix_sort_scatter.comp"));
    if (!m_RadixSortScatterShader.GetID()) return false;

    m_SweepPruneShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_sweep_prune.comp"));
    if (!m_SweepPruneShader.GetID()) return false;

    m_ResolveShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_resolve.comp"));
    if (!m_ResolveShader.GetID()) return false;

    m_CollideShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_collide.comp"));
    if (!m_CollideShader.GetID()) return false;
    
    // CRITICAL: Explicitly bind uniform blocks after shader compilation
    // This is required because layout(binding=X) in GLSL may not be respected by all drivers
    m_CollideShader.Bind();
    unsigned int physicsBlock = glGetUniformBlockIndex(m_CollideShader.GetID(), "PhysicsParams");
    unsigned int collisionBlock = glGetUniformBlockIndex(m_CollideShader.GetID(), "CollisionParams");
    if (physicsBlock != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_CollideShader.GetID(), physicsBlock, 2);
    }
    if (collisionBlock != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_CollideShader.GetID(), collisionBlock, 8);
    }

    m_ClearGridShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_clear_grid.comp"));
    if (!m_ClearGridShader.GetID()) return false;

    m_ShadersLoaded = true;
    return m_ShadersLoaded;
}

size_t GPUPhysicsWorld::InitializeCloth(int width, int height, float sx, float sy, float sz, float len, bool pinned) {
    size_t numParticles = (width + 1) * (height + 1);
    unsigned int clothID = (unsigned int)m_ClothCount++;

    std::vector<glm::vec4> pos(numParticles);
    std::vector<glm::vec4> prevPos(numParticles);
    std::vector<glm::vec4> vel(numParticles);

    for (int y = 0; y <= height; ++y) {
        for (int x = 0; x <= width; ++x) {
            size_t idx = y * (width + 1) + x;
            glm::vec3 p(sx + x * len, sy, sz + y * len);
            pos[idx] = glm::vec4(p, 1.0f);
            prevPos[idx] = glm::vec4(p, (float)clothID);
            vel[idx] = glm::vec4(0.0f, 0.0f, 0.0f, pinned ? 1.0f : 0.0f);
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PosBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, m_TotalParticles * sizeof(glm::vec4), numParticles * sizeof(glm::vec4), pos.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PrevPosBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, m_TotalParticles * sizeof(glm::vec4), numParticles * sizeof(glm::vec4), prevPos.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_VelBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, m_TotalParticles * sizeof(glm::vec4), numParticles * sizeof(glm::vec4), vel.data());

    // Generate Constraints
    std::vector<ConstraintDataType> constraints;
    auto addConstraint = [&](int p1, int p2, float stiff, float type) {
        ConstraintDataType c;
        c.indices = glm::ivec4(m_TotalParticles + p1, m_TotalParticles + p2, 0, 0);
        glm::vec3 p1pos = glm::vec3(pos[p1]);
        glm::vec3 p2pos = glm::vec3(pos[p2]);
        c.params = glm::vec2(glm::distance(p1pos, p2pos), stiff);
        c.padding = glm::vec2(type, 0.0f); // Store type
        constraints.push_back(c);
    };

    for (int y = 0; y <= height; y++) {
        for (int x = 0; x <= width; x++) {
            int i = y * (width + 1) + x;
            
            // 1. Structural (0.0)
            if (x < width) addConstraint(i, i + 1, m_Config.structuralStiffness, 0.0f);
            if (y < height) addConstraint(i, i + width + 1, m_Config.structuralStiffness, 0.0f);
            
            // 2. Shear (1.0)
            if (x < width && y < height) {
                addConstraint(i, i + width + 2, m_Config.shearStiffness, 1.0f);
                addConstraint(i + 1, i + width + 1, m_Config.shearStiffness, 1.0f);
            }
            
            // 3. Bend (2.0) - Second neighbor
            if (x < width - 1) addConstraint(i, i + 2, m_Config.bendStiffness, 2.0f);
            if (y < height - 1) addConstraint(i, i + 2 * (width + 1), m_Config.bendStiffness, 2.0f);

            // 4. Skip/Long Range (3.0) - Fourth neighbor
            if (x < width - 3) addConstraint(i, i + 4, m_Config.skipStiffness, 3.0f);
            if (y < height - 3) addConstraint(i, i + 4 * (width + 1), m_Config.skipStiffness, 3.0f);

            // 5. ULTRA-Skip (4.0) - Eighth neighbor (THE WATER KILLER)
            // Only add more if the resolution is high enough to avoid making the fabric too stiff at LOW settings.
            if (width >= 100) {
                if (x < width - 7) addConstraint(i, i + 8, m_Config.skipStiffness * 0.8f, 4.0f);
                if (y < height - 7) addConstraint(i, i + 8 * (width + 1), m_Config.skipStiffness * 0.8f, 4.0f);
            }
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, m_TotalConstraints * sizeof(ConstraintDataType), constraints.size() * sizeof(ConstraintDataType), constraints.data());

    size_t offset = m_TotalParticles;
    m_TotalParticles += numParticles;
    m_TotalConstraints += constraints.size();

    // Set initial pinned state
    SetParticlesPinned(offset, numParticles, pinned);

    // Initialize sorted indices for the new particles
    std::vector<unsigned int> sortedIndices(numParticles);
    for (unsigned int i = 0; i < (unsigned int)numParticles; i++) {
        sortedIndices[i] = (unsigned int)(offset + i);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SortedIndicesBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset * sizeof(unsigned int), numParticles * sizeof(unsigned int), sortedIndices.data());

    // Rebuild adjacency list whenever a new cloth is added
    BuildConstraintAdjacencyList();

    return offset;
}

void GPUPhysicsWorld::SetParticlesPinned(size_t start, size_t count, bool pinned) {
    if (!m_PinnedFlagsBuffer) return;
    std::vector<float> flags(count, pinned ? 1.0f : 0.0f);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, start * sizeof(float), count * sizeof(float), flags.data());
}

void GPUPhysicsWorld::Update(float deltaTime) {
    if (!m_Initialized || m_TotalParticles == 0) return;

    if (m_TotalParticles > MAX_PARTICLES) return;

    unsigned int totalGroups = (unsigned int)(m_TotalParticles + 127) / 128;
    unsigned int numBlocks = (unsigned int)(m_TotalParticles + 255) / 256;

    // 1. Initial Integration (Verlet)
    UpdateUniforms(deltaTime);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_UniformBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, 8, m_CollisionUniformBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_PrevPosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_PinnedFlagsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, m_GridHeadBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, m_GridNextBuffer);

    if (m_Terrain && m_TerrainHeightmapTexture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_TerrainHeightmapTexture);
    }

    // Clear grid and Integrate once per frame
    m_ClearGridShader.Bind();
    glDispatchCompute((32768u + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    m_IntegrateShader.Bind();
    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);


    // 2. BROADPHASE: Global Radix Sort (X-axis) - ONLY ONCE PER FRAME
    // ============================================================
    for (unsigned int bit = 0; bit < 32; bit += 4) {
        m_RadixSortHistShader.Bind();
        m_RadixSortHistShader.SetUint("bitOffset", bit);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, m_SortedIndicesBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, m_SortTempBuffer);
        glDispatchCompute(numBlocks, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        m_RadixSortScanShader.Bind();
        m_RadixSortScanShader.SetUint("numBlocks", numBlocks);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        m_RadixSortScatterShader.Bind();
        m_RadixSortScatterShader.SetUint("bitOffset", bit);
        glDispatchCompute(numBlocks, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glCopyNamedBufferSubData(m_SortTempBuffer, m_SortedIndicesBuffer, 0, 0, m_TotalParticles * sizeof(unsigned int));
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Sweep & Prune Broadphase
    unsigned int zero = 0;
    size_t histOffset = m_TotalParticles;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SortTempBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, (histOffset + 8192) * sizeof(unsigned int), sizeof(unsigned int), &zero);
    
    m_SweepPruneShader.Bind();
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, m_SortedIndicesBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, m_SortTempBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 15, m_CollisionPairsBuffer);
    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // 3. NARROWPHASE & SOLVER SUBSTEPPING
    // ============================================================
    int substeps = std::max<int>(1, m_Config.collisionSubsteps);
    
    for (int s = 0; s < substeps; s++) {
        // A. Resolve inter-cloth collisions (PBD style)
        m_ResolveShader.Bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_PrevPosBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, m_SortTempBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 15, m_CollisionPairsBuffer);
        glDispatchCompute(2048, 1, 1); 
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // B. Constraint Solving (Structural stiffness)
        m_SolveShader.Bind();
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_ConstraintBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_ParticleColorsBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, m_ConstraintAdjacencyBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, m_ConstraintIndicesBuffer);
        
        int solverIters = std::max<int>(1, m_Config.iterations / substeps);
        for (int iter = 0; iter < solverIters; iter++) {
            for (int color = 0; color < MAX_COLORS; color++) {
                m_SolveShader.SetInt("current_color", color);
                glDispatchCompute(numBlocks, 1, 1);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            }
        }

        // C. Sphere and Ground Collision - RUN LAST (Final word on position)
        m_CollideShader.Bind();
        glDispatchCompute(numBlocks, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

void GPUPhysicsWorld::Unbind() const {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GPUPhysicsWorld::UpdateUniforms(float deltaTime) {
    if (!m_UniformBuffer || !m_CollisionUniformBuffer) return;

    // Physics Parameters (binding 2)
    struct PhysicsParams {
        glm::vec4 gravity_dt;        // xyz = gravity, w = deltaTime
        glm::vec4 params1;           // x = damping, y = numParticles, z = numConstraints, w = restLength
        glm::vec4 terrain;           // xy = terrainSize, z = terrainHeightScale, w = gravityScale
        glm::vec4 forces;            // x = airResistance, y = windStrength, zw = padding
        glm::vec4 windDir_stretch;   // xyz = windDirection, w = stretchResistance
        glm::vec4 limits;            // x = maxVelocity, y = selfCollisionRadius, z = selfCollisionStrength, w = padding
    } p;

    p.gravity_dt = glm::vec4(m_Config.gravity, deltaTime);
    p.params1 = glm::vec4(m_Config.damping, (float)m_TotalParticles, (float)m_TotalConstraints, 0.1f); 
    
    glm::vec2 tSize = m_Terrain ? m_Terrain->GetSize() : glm::vec2(0.0f);
    float tHeight = m_Terrain ? m_Terrain->GetHeightScale() : 0.0f;
    p.terrain = glm::vec4(tSize.x, tSize.y, tHeight, m_Config.gravityScale);
    
    p.forces = glm::vec4(m_Config.airResistance, m_Config.windStrength, 0.0f, 0.0f);
    p.windDir_stretch = glm::vec4(m_Config.windDirection.x, m_Config.windDirection.y, m_Config.windDirection.z, m_Config.structuralStiffness);
    p.limits = glm::vec4(m_Config.maxVelocity, m_Config.selfCollisionRadius, m_Config.selfCollisionStrength, 0.0f);

    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PhysicsParams), &p);

    // Collision Parameters (binding 3)
    struct CollisionParamsData {
        glm::vec3 sphereCenter; float padding1;
        float sphereRadius;
        float groundLevel;
        float collisionMargin;
        float dampingFactor;
        float frictionFactor;
        int collisionSubsteps;
        float sphereStaticFriction;
        float sphereDynamicFriction;
        float sphereBounce;
        float sphereGripFactor;
        float staticFrictionThreshold;
        float padding2[3];
    } c;

    c.sphereCenter = m_Collision.sphereCenter;
    c.sphereRadius = m_Collision.sphereRadius;
    c.groundLevel = m_Collision.groundLevel;
    c.collisionMargin = m_Config.collisionMargin;
    c.dampingFactor = m_Config.dampingFactor;
    c.frictionFactor = m_Config.frictionFactor;
    c.collisionSubsteps = m_Config.collisionSubsteps;
    c.sphereStaticFriction = m_Config.sphereStaticFriction;
    c.sphereDynamicFriction = m_Config.sphereDynamicFriction;
    c.sphereBounce = m_Config.sphereBounce;
    c.sphereGripFactor = m_Config.sphereGripFactor;
    c.staticFrictionThreshold = m_Config.staticFrictionThreshold;

    glBindBuffer(GL_UNIFORM_BUFFER, m_CollisionUniformBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CollisionParamsData), &c);
}

void GPUPhysicsWorld::SetCollisionSphere(const glm::vec3& center, float radius) {
    m_Collision.sphereCenter = center;
    m_Collision.sphereRadius = radius;
    UpdateUniforms(1.0f/60.0f);
}

void GPUPhysicsWorld::SetQualityLevel(int iterations, bool useGather) {
    m_MaxIterations = iterations; m_UseTextureGather = useGather;
}

void GPUPhysicsWorld::CreateTerrainHeightmapTexture() {
    if (!m_Terrain) return;

    if (m_TerrainHeightmapTexture) glDeleteTextures(1, &m_TerrainHeightmapTexture);

    glGenTextures(1, &m_TerrainHeightmapTexture);
    glBindTexture(GL_TEXTURE_2D, m_TerrainHeightmapTexture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const std::vector<float>& heightData = m_Terrain->GetHeightData();
    int size = (int)sqrt(heightData.size());
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, size, size, 0, GL_RED, GL_FLOAT, heightData.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
void GPUPhysicsWorld::BuildConstraintAdjacencyList() {
    if (m_TotalParticles == 0 || m_TotalConstraints == 0) return;

    // 1. Read back all constraints from GPU (or keep a CPU copy)
    std::vector<ConstraintDataType> constraints(m_TotalConstraints);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintBuffer);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_TotalConstraints * sizeof(ConstraintDataType), constraints.data());

    // 2. Count constraints per particle
    std::vector<std::vector<int>> adj(m_TotalParticles);
    for (int i = 0; i < (int)m_TotalConstraints; i++) {
        if (constraints[i].indices.x < (int)m_TotalParticles)
            adj[constraints[i].indices.x].push_back(i);
        if (constraints[i].indices.y < (int)m_TotalParticles)
            adj[constraints[i].indices.y].push_back(i);
    }

    // 3. Build flattened adjacency buffers
    std::vector<glm::ivec2> particleRanges(m_TotalParticles);
    std::vector<int> flatIndices;
    flatIndices.reserve(m_TotalConstraints * 2);

    for (int i = 0; i < (int)m_TotalParticles; i++) {
        particleRanges[i] = glm::ivec2((int)flatIndices.size(), (int)adj[i].size());
        for (int cIdx : adj[i]) {
            flatIndices.push_back(cIdx);
        }
    }

    // 4. Graph Coloring (Simple greedy approach)
    std::vector<unsigned int> colors(m_TotalParticles, 0);
    for (int i = 0; i < (int)m_TotalParticles; i++) {
        std::uint32_t mask = 0;
        for (int cIdx : adj[i]) {
            int other = (constraints[cIdx].indices.x == i) ? constraints[cIdx].indices.y : constraints[cIdx].indices.x;
            if (other < i && other >= 0) { // Already colored
                unsigned int otherColor = colors[other];
                if (otherColor < 31) { // Safety check for 32-bit bitmask
                    mask |= (1 << otherColor);
                }
            }
        }
        // Find first free color
        unsigned int color = 0;
        while ((mask & (1 << color)) && color < (unsigned int)MAX_COLORS - 1) color++;
        colors[i] = color;
    }

    // Upload to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintAdjacencyBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_TotalParticles * sizeof(glm::ivec2), particleRanges.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintIndicesBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, flatIndices.size() * sizeof(int), flatIndices.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleColorsBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_TotalParticles * sizeof(unsigned int), colors.data());
}
void GPUPhysicsWorld::AutoTuneSettings() {}
void GPUPhysicsWorld::SetConfig(const GPUPhysicsConfig& config) { m_Config = config; }

} // namespace cloth
