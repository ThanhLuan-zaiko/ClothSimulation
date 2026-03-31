#include "GPUPhysicsWorld.h"
#include "AppState.h"
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

extern AppState* g_State;

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
      m_BendingConstraintBuffer(0),
      m_DeltaPositionBuffer(0),
      m_ParticleDataBuffer(0),
      m_PinnedFlagsBuffer(0),
      m_ConstraintIndicesBuffer(0),
      m_GridHeadBuffer(0),
      m_GridNextBuffer(0),
      m_UniformBuffer(0),
      m_CollisionUniformBuffer(0),
      m_SortedIndicesBuffer(0),
      m_SortTempBuffer(0),
      m_CollisionPairsBuffer(0),
      m_GridHeaderBuffer(0),       // NEW: Spatial Hash Grid
      m_GridItemsBuffer(0),        // NEW: Spatial Hash Grid
      m_PairCountBuffer(0),        // NEW: Atomic counter
      m_WorkGroupSize(128),
      m_BatchSize(4),
      m_MaxIterations(8),
      m_UseTextureGather(false),
      m_AutoTuningEnabled(true),
      m_ManualPresetSelected(false),
      m_TotalParticles(0),
      m_TotalConstraints(0),
      m_TotalBendingConstraints(0),
      m_Initialized(false) {}

GPUPhysicsWorld::~GPUPhysicsWorld() {
    if (m_UniformBuffer) glDeleteBuffers(1, &m_UniformBuffer);
    if (m_CollisionUniformBuffer) glDeleteBuffers(1, &m_CollisionUniformBuffer);
    if (m_PosBuffer) glDeleteBuffers(1, &m_PosBuffer);
    if (m_PrevPosBuffer) glDeleteBuffers(1, &m_PrevPosBuffer);
    if (m_VelBuffer) glDeleteBuffers(1, &m_VelBuffer);
    if (m_ConstraintBuffer) glDeleteBuffers(1, &m_ConstraintBuffer);
    if (m_BendingConstraintBuffer) glDeleteBuffers(1, &m_BendingConstraintBuffer);
    if (m_DeltaPositionBuffer) glDeleteBuffers(1, &m_DeltaPositionBuffer);
    if (m_ParticleDataBuffer) glDeleteBuffers(1, &m_ParticleDataBuffer);
    if (m_PinnedFlagsBuffer) glDeleteBuffers(1, &m_PinnedFlagsBuffer);
    if (m_ConstraintIndicesBuffer) glDeleteBuffers(1, &m_ConstraintIndicesBuffer);
    if (m_GridHeadBuffer) glDeleteBuffers(1, &m_GridHeadBuffer);
    if (m_GridNextBuffer) glDeleteBuffers(1, &m_GridNextBuffer);
    // NEW: Spatial Hash Grid buffers cleanup
    if (m_GridHeaderBuffer) glDeleteBuffers(1, &m_GridHeaderBuffer);
    if (m_GridItemsBuffer) glDeleteBuffers(1, &m_GridItemsBuffer);
    if (m_PairCountBuffer) glDeleteBuffers(1, &m_PairCountBuffer);
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

    // Allocate Particle Buffers (optimized bindings)
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

    glGenBuffers(1, &m_BendingConstraintBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BendingConstraintBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_CONSTRAINTS * sizeof(BendingConstraintDataType), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_DeltaPositionBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_DeltaPositionBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * 3 * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_PinnedFlagsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    // Combined ParticleData buffer: colors + adjacency
    // Size = numParticles (colors) + numParticles * 2 (adjacency ranges)
    glGenBuffers(1, &m_ParticleDataBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleDataBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * 3 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_ConstraintIndicesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintIndicesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_CONSTRAINTS * 2 * sizeof(int), nullptr, GL_DYNAMIC_DRAW);

    // Grid Buffers
    const unsigned int GRID_SIZE = 32768u;
    std::vector<unsigned int> initGrid(GRID_SIZE, 0xFFFFFFFF);
    glGenBuffers(1, &m_GridHeadBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GridHeadBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, GRID_SIZE * sizeof(unsigned int), initGrid.data(), GL_DYNAMIC_DRAW);

    std::vector<unsigned int> initNext(MAX_PARTICLES, 0xFFFFFFFF);
    glGenBuffers(1, &m_GridNextBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GridNextBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(unsigned int), initNext.data(), GL_DYNAMIC_DRAW);

    // Uniform Buffers (UBO binding 0 = PhysicsParams, binding 1 = CollisionParams)
    glGenBuffers(1, &m_UniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 160, nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_CollisionUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_CollisionUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 144, nullptr, GL_DYNAMIC_DRAW); // Updated size for new fields

    // Radix Sort / Sweep & Prune Buffers
    glGenBuffers(1, &m_SortedIndicesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SortedIndicesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &m_SortTempBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_SortTempBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * 3 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);

    const unsigned int MAX_PAIRS = 524288u;
    glGenBuffers(1, &m_CollisionPairsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CollisionPairsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PAIRS * sizeof(unsigned int) * 2, nullptr, GL_DYNAMIC_DRAW);

    // NEW: Spatial Hash Grid buffers
    const unsigned int GRID_HASH_SIZE = 65536u;
    const unsigned int GRID_MAX_ITEMS = GRID_HASH_SIZE * 64u;
    
    glGenBuffers(1, &m_GridHeaderBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GridHeaderBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, GRID_HASH_SIZE * sizeof(unsigned int) * 2, nullptr, GL_DYNAMIC_DRAW);
    
    glGenBuffers(1, &m_GridItemsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GridItemsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, GRID_MAX_ITEMS * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
    
    glGenBuffers(1, &m_PairCountBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PairCountBuffer);
    unsigned int zero = 0;
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int), &zero, GL_DYNAMIC_DRAW);

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

    m_BendShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_bend.comp"));
    if (!m_BendShader.GetID()) return false;

    m_ApplyDeltasShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_apply_deltas.comp"));
    if (!m_ApplyDeltasShader.GetID()) return false;

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

    // NEW: Load CCD solver with binary search TOI
    m_CCDSolveShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_ccd_solve.comp"));
    if (!m_CCDSolveShader.GetID()) return false;

    // NEW: Load Spatial Hash grid builder
    m_SpatialHashShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_spatial_hash.comp"));
    if (!m_SpatialHashShader.GetID()) return false;

    m_StrainLimitShader = Shader::CreateComputeShaderFromSource(preprocess("shaders/physics/modular/physics_strain_limit.comp"));
    if (!m_StrainLimitShader.GetID()) return false;

    // CRITICAL: Explicitly bind uniform blocks after shader compilation
    m_CollideShader.Bind();
    unsigned int physicsBlock = glGetUniformBlockIndex(m_CollideShader.GetID(), "PhysicsParams");
    unsigned int collisionBlock = glGetUniformBlockIndex(m_CollideShader.GetID(), "CollisionParams");
    if (physicsBlock != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_CollideShader.GetID(), physicsBlock, 0);
    }
    if (collisionBlock != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_CollideShader.GetID(), collisionBlock, 1);
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

    // 6. Generate Dihedral Bending Constraints (NEW)
    std::vector<BendingConstraintDataType> bendConstraints;
    auto addBend = [&](int p1, int p2, int p3, int p4, float stiff, float anisotropic) {
        BendingConstraintDataType bc;
        bc.indices = glm::ivec4(m_TotalParticles + p1, m_TotalParticles + p2, m_TotalParticles + p3, m_TotalParticles + p4);
        bc.params = glm::vec2(0.0f, stiff); // restAngle initially 0 for flat cloth
        bc.padding = glm::vec2(anisotropic, 0.0f);
        bendConstraints.push_back(bc);
    };

    for (int y = 0; y <= height; y++) {
        for (int x = 0; x <= width; x++) {
            // Diagonal edges (Internal to each quad)
            if (x < width && y < height) {
                int p1 = (y + 1) * (width + 1) + x;
                int p2 = y * (width + 1) + (x + 1);
                int p3 = y * (width + 1) + x;
                int p4 = (y + 1) * (width + 1) + (x + 1);
                addBend(p1, p2, p3, p4, m_Config.bendStiffness, 1.0f); // Diagonal is neutral
            }
            // Horizontal edges (Between vertical quads)
            if (x < width && y > 0 && y < height) {
                int p1 = y * (width + 1) + x;
                int p2 = y * (width + 1) + (x + 1);
                int p3 = (y - 1) * (width + 1) + (x + 1);
                int p4 = (y + 1) * (width + 1) + x;
                addBend(p1, p2, p3, p4, m_Config.bendStiffness, 1.2f); // Warp direction (stiffer)
            }
            // Vertical edges (Between horizontal quads)
            if (y < height && x > 0 && x < width) {
                int p1 = y * (width + 1) + x;
                int p2 = (y + 1) * (width + 1) + x;
                int p3 = (y + 1) * (width + 1) + (x - 1);
                int p4 = y * (width + 1) + (x + 1);
                addBend(p1, p2, p3, p4, m_Config.bendStiffness, 0.8f); // Weft direction (softer)
            }
        }
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_BendingConstraintBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, m_TotalBendingConstraints * sizeof(BendingConstraintDataType), bendConstraints.size() * sizeof(BendingConstraintDataType), bendConstraints.data());

    size_t offset = m_TotalParticles;
    m_TotalParticles += numParticles;
    m_TotalConstraints += constraints.size();
    m_TotalBendingConstraints += bendConstraints.size();

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

void GPUPhysicsWorld::ResetCloth(size_t offset, int width, int height, float sx, float sy, float sz, float len, bool pinned, int clothID) {
    size_t numParticles = (width + 1) * (height + 1);
    
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
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset * sizeof(glm::vec4), numParticles * sizeof(glm::vec4), pos.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PrevPosBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset * sizeof(glm::vec4), numParticles * sizeof(glm::vec4), prevPos.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_VelBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset * sizeof(glm::vec4), numParticles * sizeof(glm::vec4), vel.data());

    // Reset pinned state
    SetParticlesPinned(offset, numParticles, pinned);
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

    // ========== DEBUG: Check deltaTime ==========
    // If deltaTime is too large or zero, physics will break
    if (deltaTime <= 0.0f || deltaTime > 0.5f) {
        // Skip this frame if deltaTime is invalid
        return;
    }

    unsigned int totalGroups = (unsigned int)(m_TotalParticles + 127) / 128;
    unsigned int numBlocks = (unsigned int)(m_TotalParticles + 255) / 256;

    // Update UBOs
    UpdateUniforms(deltaTime);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_UniformBuffer);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_CollisionUniformBuffer);

    if (m_Terrain && m_TerrainHeightmapTexture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_TerrainHeightmapTexture);
    }

    // ========================================================================
    // PHASE 1: Integration (Verlet)
    // ========================================================================
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_PrevPosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_PinnedFlagsBuffer);

    m_IntegrateShader.Bind();
    glDispatchCompute(numBlocks, 1, 1);
    // GL_SHADER_STORAGE_BARRIER_BIT is sufficient for SSBO synchronization
    // GL_SHADER_IMAGE_BARRIER_BIT = 0x20 (not needed unless using imageLoad/imageStore)
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    // ========================================================================
    // PHASE 2: BROADPHASE - Spatial Hash Grid
    // ========================================================================
    // Reset grid header and pair count
    const unsigned int GRID_HASH_SIZE = 65536u;
    std::vector<unsigned int> clearGrid(GRID_HASH_SIZE * 2, 0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_GridHeaderBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, clearGrid.size() * sizeof(unsigned int), clearGrid.data());
    
    unsigned int zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PairCountBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(unsigned int), &zero);

    // Bind spatial hash buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 14, m_GridHeaderBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 15, m_GridItemsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, m_CollisionPairsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, m_PairCountBuffer);

    // Build spatial hash grid
    m_SpatialHashShader.Bind();
    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ========================================================================
    // PHASE 3: CCD COLLISION with Binary Search TOI
    // ========================================================================
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_PrevPosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_DeltaPositionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_PinnedFlagsBuffer);

    m_CCDSolveShader.Bind();
    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ========================================================================
    // PHASE 4: Constraint Solving (Graph Coloring) - Multiple iterations
    // ========================================================================
    int iterations = m_Config.iterations;
    for (int i = 0; i < iterations; i++) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_PrevPosBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_ConstraintBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_PinnedFlagsBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_ParticleDataBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_ConstraintIndicesBuffer);

        m_SolveShader.Bind();
        glDispatchCompute(numBlocks, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ========================================================================
    // PHASE 5: Dihedral Bending Solver
    // ========================================================================
    if (m_TotalBendingConstraints > 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_PrevPosBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_BendingConstraintBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_DeltaPositionBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_PinnedFlagsBuffer);

        m_BendShader.Bind();
        m_BendShader.SetInt("numBendingConstraints", (int)m_TotalBendingConstraints);
        glDispatchCompute((unsigned int)(m_TotalBendingConstraints + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // ========================================================================
    // PHASE 6: Strain Limiting
    // ========================================================================
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_PrevPosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_ConstraintBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_DeltaPositionBuffer);

    m_StrainLimitShader.Bind();
    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ========================================================================
    // PHASE 7: Apply Delta Positions (accumulated from collisions)
    // ========================================================================
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_DeltaPositionBuffer);

    m_ApplyDeltasShader.Bind();
    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // ========================================================================
    // PHASE 8: Final Velocity Clamping (safety)
    // ========================================================================
    // Re-bind collision shader for final velocity check
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_PosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_PrevPosBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_PinnedFlagsBuffer);

    m_CollideShader.Bind();
    glDispatchCompute(numBlocks, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void GPUPhysicsWorld::Unbind() const {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GPUPhysicsWorld::UpdateUniforms(float deltaTime) {
    if (!m_UniformBuffer || !m_CollisionUniformBuffer) return;

    // Physics Parameters (UBO binding 0)
    struct PhysicsParams {
        glm::vec4 gravity_dt;
        glm::vec4 params1;
        glm::vec4 terrain;
        glm::vec4 forces;
        glm::vec4 windDir_stretch;
        glm::vec4 limits;
        glm::vec4 interaction;
        glm::vec4 time_data;
    } p;

    p.gravity_dt = glm::vec4(m_Config.gravity, deltaTime);
    p.params1 = glm::vec4(m_Config.damping, (float)m_TotalParticles, (float)m_TotalConstraints, 0.1f);

    glm::vec2 tSize = m_Terrain ? m_Terrain->GetSize() : glm::vec2(0.0f);
    float tHeight = m_Terrain ? m_Terrain->GetHeightScale() : 0.0f;
    p.terrain = glm::vec4(tSize.x, tSize.y, tHeight, m_Config.gravityScale);

    p.forces = glm::vec4(m_Config.airResistance, m_Config.windStrength, 0.0f, 0.0f);
    p.windDir_stretch = glm::vec4(m_Config.windDirection.x, m_Config.windDirection.y, m_Config.windDirection.z, m_Config.structuralStiffness);
    p.limits = glm::vec4(m_Config.maxVelocity, m_Config.selfCollisionRadius, m_Config.selfCollisionStrength, 0.0f);

    if (g_State) {
        p.interaction = glm::vec4(g_State->interactionWorldPos, g_State->isInteracting ? 1.0f : 0.0f);
        p.time_data = glm::vec4(g_State->totalTime, 0.0f, 0.0f, 0.0f);
    } else {
        p.interaction = glm::vec4(0.0f);
        p.time_data = glm::vec4(0.0f);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PhysicsParams), &p);

    // Collision Parameters (UBO binding 1)
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
        float sleepingThreshold;
        float sphereWrapFactor;
        float terrainDamping;
        float ccdSubsteps;
        float conservativeFactor;
        float maxPenetrationCorrection;
        float padding2[1];
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
    c.sleepingThreshold = m_Config.sleepingThreshold;
    c.sphereWrapFactor = m_Config.sphereWrapFactor;
    c.terrainDamping = m_Config.terrainDamping;
    c.ccdSubsteps = m_Config.ccdSubsteps;
    c.conservativeFactor = m_Config.conservativeFactor;
    c.maxPenetrationCorrection = m_Config.maxPenetrationCorrection;

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

    // 1. Read back all constraints
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

    // 3. Build flattened adjacency indices
    std::vector<int> flatIndices;
    flatIndices.reserve(m_TotalConstraints * 2);

    for (int i = 0; i < (int)m_TotalParticles; i++) {
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
            if (other < i && other >= 0) {
                unsigned int otherColor = colors[other];
                if (otherColor < 31) {
                    mask |= (1 << otherColor);
                }
            }
        }
        unsigned int color = 0;
        while ((mask & (1 << color)) && color < (unsigned int)MAX_COLORS - 1) color++;
        colors[i] = color;
    }

    // 5. Upload to combined ParticleData buffer
    // Layout: [0..numParticles-1] = colors
    //         [numParticles..end] = adjacency ranges (start, count) per particle
    std::vector<unsigned int> particleData(m_TotalParticles * 3);
    
    // First: colors
    for (size_t i = 0; i < m_TotalParticles; i++) {
        particleData[i] = colors[i];
    }
    
    // Then: adjacency ranges (start, count)
    int currentIndex = 0;
    for (size_t i = 0; i < m_TotalParticles; i++) {
        particleData[m_TotalParticles + i * 2] = (unsigned int)currentIndex;
        particleData[m_TotalParticles + i * 2 + 1] = (unsigned int)adj[i].size();
        currentIndex += (int)adj[i].size();
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleDataBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_TotalParticles * 3 * sizeof(unsigned int), particleData.data());

    // Upload constraint indices separately
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintIndicesBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, flatIndices.size() * sizeof(int), flatIndices.data());
}

void GPUPhysicsWorld::AutoTuneSettings() {}
void GPUPhysicsWorld::SetConfig(const GPUPhysicsConfig& config) { m_Config = config; }

} // namespace cloth
