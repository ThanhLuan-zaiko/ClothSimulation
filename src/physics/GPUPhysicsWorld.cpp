#include "GPUPhysicsWorld.h"
#include "utils/GPUDetection.h"
#include "cloth/Cloth.h"
#include <glad/glad.h>
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
    : m_UniformBuffer(0),
      m_CollisionUniformBuffer(0),
      m_PinnedFlagsBuffer(0),
      m_CollisionCountBuffer(0),
      m_CollisionDataBuffer(0),
      m_ParticleColorsBuffer(0),
      m_ColorCollisionCountBuffer(0),
      m_ConstraintAdjacencyBuffer(0),
      m_ConstraintIndicesBuffer(0),
      m_ShaderLoaded(false),
      m_ShaderValidated(false),
      m_IsHighEndGPU(false),
      m_IsIntelGPU(false),
      m_Terrain(nullptr),
      m_TerrainHeightmapTexture(0),
      m_WorkGroupSize(128),
      m_BatchSize(4),
      m_PhysicsParamsBlockIndex(-1),
      m_CollisionParamsBlockIndex(-1),
      m_MaxIterations(2),
      m_UseTextureGather(false),
      m_AutoTuningEnabled(true),
      m_ManualPresetSelected(false),
      m_TotalParticles(0),
      m_TotalConstraints(0),
      m_Initialized(false) {
}

GPUPhysicsWorld::~GPUPhysicsWorld() {
    // Uniform buffers
    if (m_UniformBuffer != 0) {
        glDeleteBuffers(1, &m_UniformBuffer);
    }
    if (m_CollisionUniformBuffer != 0) {
        glDeleteBuffers(1, &m_CollisionUniformBuffer);
    }

    // Atomic collision buffers
    if (m_CollisionCountBuffer != 0) {
        glDeleteBuffers(1, &m_CollisionCountBuffer);
    }
    if (m_CollisionDataBuffer != 0) {
        glDeleteBuffers(1, &m_CollisionDataBuffer);
    }
    
    // Graph coloring buffers
    if (m_ParticleColorsBuffer != 0) {
        glDeleteBuffers(1, &m_ParticleColorsBuffer);
    }
    if (m_ColorCollisionCountBuffer != 0) {
        glDeleteBuffers(1, &m_ColorCollisionCountBuffer);
    }

    // Constraint adjacency buffer
    if (m_ConstraintAdjacencyBuffer != 0) {
        glDeleteBuffers(1, &m_ConstraintAdjacencyBuffer);
    }

    // Constraint indices buffer
    if (m_ConstraintIndicesBuffer != 0) {
        glDeleteBuffers(1, &m_ConstraintIndicesBuffer);
    }

    // Terrain texture
    if (m_TerrainHeightmapTexture != 0) {
        glDeleteTextures(1, &m_TerrainHeightmapTexture);
    }
}

bool GPUPhysicsWorld::Initialize(const GPUPhysicsConfig& config) {
    m_Config = config;

    // Detect GPU type
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    m_IsHighEndGPU = (vendor && (strstr(vendor, "NVIDIA") || strstr(vendor, "AMD")));
    m_IsIntelGPU = (vendor && strstr(vendor, "Intel"));
    
    std::cout << "[GPU] Detected: " << (vendor ? vendor : "Unknown") << std::endl;
    std::cout << "[GPU] High-end (NVIDIA/AMD): " << (m_IsHighEndGPU ? "YES" : "NO") << std::endl;
    std::cout << "[GPU] Intel Integrated: " << (m_IsIntelGPU ? "YES" : "NO") << std::endl;

    // Load compute shader with GPU-specific optimizations
    if (!LoadComputeShader()) {
        std::cerr << "[GPUPhysicsWorld] Failed to load compute shader!" << std::endl;
        return false;
    }

    // Get work group size from compute shader
    GLint workGroupSize[3];
    glGetProgramiv(m_ComputeShader.GetID(), GL_COMPUTE_WORK_GROUP_SIZE, workGroupSize);
    m_WorkGroupSize = static_cast<unsigned int>(workGroupSize[0]);
    m_BatchSize = 4;  // Default conservative value
    
    // Cache uniform block indices (so we don't call glGetUniformBlockIndex every frame)
    m_PhysicsParamsBlockIndex = glGetUniformBlockIndex(m_ComputeShader.GetID(), "PhysicsParams");
    m_CollisionParamsBlockIndex = glGetUniformBlockIndex(m_ComputeShader.GetID(), "CollisionParams");

    if (m_PhysicsParamsBlockIndex == GL_INVALID_INDEX) {
        std::cerr << "[GPUPhysicsWorld] WARNING: PhysicsParams uniform block not found!" << std::endl;
    } else {
        // Set binding point 2 for PhysicsParams (NOT 0 - SSBOs use 0 and 1)
        glUniformBlockBinding(m_ComputeShader.GetID(), m_PhysicsParamsBlockIndex, 2);
    }

    if (m_CollisionParamsBlockIndex == GL_INVALID_INDEX) {
        std::cerr << "[GPUPhysicsWorld] WARNING: CollisionParams uniform block not found!" << std::endl;
    } else {
        // Set binding point 3 for CollisionParams (NOT 1 - SSBOs use 0 and 1)
        glUniformBlockBinding(m_ComputeShader.GetID(), m_CollisionParamsBlockIndex, 3);
    }

    // Create uniform buffers with CORRECT sizes for std140 layout
    // PhysicsParams: vec3(16) + float(4) + float(4) + int(4) + int(4) + float(4) + vec2(8) + float(4) = 52 bytes → 64 bytes aligned
    glGenBuffers(1, &m_UniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 64, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // CollisionParams: vec3(16) + float(4) + float(4) + float(4) + float(4) + float(4) + int(4) = 40 bytes → 48 bytes aligned
    glGenBuffers(1, &m_CollisionUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_CollisionUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, 48, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    UpdateUniforms(1.0f / 60.0f);

    // Initialize atomic collision buffers
    glGenBuffers(1, &m_CollisionCountBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CollisionCountBuffer);
    unsigned int initialCollisionCount = 0;
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int), &initialCollisionCount, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Collision data buffer (max 10000 collisions)
    glGenBuffers(1, &m_CollisionDataBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CollisionDataBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 10000 * sizeof(unsigned int), nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Initialize graph coloring buffers
    glGenBuffers(1, &m_ParticleColorsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleColorsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * sizeof(unsigned int), nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Color collision counters (one per color) - now supports 16 colors
    glGenBuffers(1, &m_ColorCollisionCountBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ColorCollisionCountBuffer);
    unsigned int colorCounts[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16 * sizeof(unsigned int), colorCounts, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Constraint adjacency list buffer (will be built after cloth initialization)
    glGenBuffers(1, &m_ConstraintAdjacencyBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintAdjacencyBuffer);
    // Each entry: ivec2 (offset, count) - 8 bytes per particle
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * 2 * sizeof(int), nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Constraint indices buffer (flat array)
    glGenBuffers(1, &m_ConstraintIndicesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintIndicesBuffer);
    // Max 24 constraints per particle × MAX_PARTICLES (support double bend/shear)
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_PARTICLES * 24 * sizeof(int), nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // IMPORTANT: Set initialized flag
    m_Initialized = true;

    // Run auto-tune benchmark ONLY if user didn't manually select a preset
    if (m_AutoTuningEnabled && !m_ManualPresetSelected) {
        AutoTuneSettings();
    } else if (m_ManualPresetSelected) {
        std::cout << "[GPU] Manual preset selected - using preset settings: iterations=" << m_MaxIterations 
                  << ", textureGather=" << (m_UseTextureGather ? "YES" : "NO") << std::endl;
    }

    return true;
}

bool GPUPhysicsWorld::LoadComputeShader() {
    // Load shader source
    std::ifstream file("shaders/physics/physics.comp");
    if (!file.is_open()) {
        std::cerr << "[GPUPhysicsWorld] Failed to open shader file!" << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string shaderSource = buffer.str();
    file.close();

    // GPU-specific optimizations based on auto-tune OR vendor detection
    if (m_UseTextureGather || m_IsHighEndGPU) {
        // High-end: Enable textureGather
        std::cout << "[GPU] Using HIGH-END shader path (textureGather)" << std::endl;
        shaderSource = replace(shaderSource, "#define USE_TEXTURE_GATHER 0", "#define USE_TEXTURE_GATHER 1");
    } else {
        // Integrated: Use optimized path
        std::cout << "[GPU] Using INTEGRATED shader path (textureLod)" << std::endl;
    }
    
    // Apply auto-tuned iteration count
    std::cout << "[GPU] MAX_ITERATIONS = " << m_MaxIterations << std::endl;
    shaderSource = replace(shaderSource, "#define MAX_ITERATIONS_RUNTIME 2", 
                           "#define MAX_ITERATIONS_RUNTIME " + std::to_string(m_MaxIterations));

    // Compile shader from source
    m_ComputeShader = Shader::CreateComputeShaderFromSource(shaderSource);

    if (m_ComputeShader.GetID() != 0) {
        m_ShaderLoaded = true;
        return true;
    }

    std::cerr << "[GPUPhysicsWorld] Failed to compile compute shader!" << std::endl;
    return false;
}

// Helper function to replace strings
std::string GPUPhysicsWorld::replace(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

size_t GPUPhysicsWorld::InitializeCloth(int widthSegments, int heightSegments,
                                        float startX, float startY, float startZ,
                                        float segmentLength, bool pinned) {
    // Calculate particle count
    int numParticles = (widthSegments + 1) * (heightSegments + 1);
    
    // Count constraints accurately (with DOUBLE shear and bend)
    int numConstraints = 0;
    
    // Structural (horizontal + vertical)
    numConstraints += widthSegments * (heightSegments + 1);  // Horizontal
    numConstraints += heightSegments * (widthSegments + 1);  // Vertical
    
    // Shear (DOUBLE: primary + secondary + skip-2)
    numConstraints += widthSegments * heightSegments * 2;  // Primary + Secondary
    // Skip-2 shear (only when x < widthSegments-1 && y < heightSegments-1)
    if (widthSegments > 1 && heightSegments > 1) {
        numConstraints += (widthSegments - 1) * (heightSegments - 1) * 2;
    }
    
    // Bend (DOUBLE: skip-1 + skip-2)
    // Skip-1 bend
    numConstraints += (widthSegments - 1) * (heightSegments + 1);  // Horizontal
    numConstraints += (widthSegments + 1) * (heightSegments - 1);  // Vertical
    // Skip-2 bend
    if (widthSegments > 2) {
        numConstraints += (widthSegments - 2) * (heightSegments + 1);  // Horizontal
    }
    if (heightSegments > 2) {
        numConstraints += (widthSegments + 1) * (heightSegments - 2);  // Vertical
    }

    size_t particleOffset = m_TotalParticles;
    size_t constraintOffset = m_TotalConstraints;

    // Create particles using new interleaved ParticleData structure
    std::vector<ParticleData> particles(numParticles);
    for (int y = 0; y <= heightSegments; y++) {
        for (int x = 0; x <= widthSegments; x++) {
            int idx = y * (widthSegments + 1) + x;
            particles[idx].position = glm::vec4(
                startX + x * segmentLength,
                startY,
                startZ + y * segmentLength,
                1.0f  // mass
            );
            particles[idx].prevPosition = particles[idx].position;
            particles[idx].velocity = glm::vec4(0.0f, 0.0f, 0.0f, pinned ? 1.0f : 0.0f);
        }
    }

    // Create constraints using new interleaved ConstraintData structure
    std::vector<ConstraintData> constraints(numConstraints);
    size_t cIdx = 0;

    float stiffness = 1.0f;

    // Structural constraints
    for (int y = 0; y <= heightSegments; y++) {
        for (int x = 0; x < widthSegments; x++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + 1;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 0, 0);
            constraints[cIdx].params = glm::vec2(segmentLength, stiffness);
            cIdx++;
        }
    }

    for (int y = 0; y < heightSegments; y++) {
        for (int x = 0; x <= widthSegments; x++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + (widthSegments + 1);
            constraints[cIdx].indices = glm::ivec4(p1, p2, 0, 0);
            constraints[cIdx].params = glm::vec2(segmentLength, stiffness);
            cIdx++;
        }
    }

    // Shear constraints (DOUBLE: add both diagonal directions)
    for (int y = 0; y < heightSegments; y++) {
        for (int x = 0; x < widthSegments; x++) {
            // Primary shear (bottom-left to top-right)
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + (widthSegments + 1) + 1;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 1, 0);
            constraints[cIdx].params = glm::vec2(glm::length(glm::vec3(segmentLength, 0, segmentLength)), 0.5f);
            cIdx++;

            // Secondary shear (bottom-right to top-left)
            p1 = y * (widthSegments + 1) + x + 1;
            p2 = p1 + (widthSegments + 1) - 1;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 1, 0);
            constraints[cIdx].params = glm::vec2(glm::length(glm::vec3(segmentLength, 0, segmentLength)), 0.5f);
            cIdx++;
            
            // Additional shear (skip 2 particles for extra stability)
            if (x < widthSegments - 1 && y < heightSegments - 1) {
                p1 = y * (widthSegments + 1) + x;
                p2 = p1 + 2 * (widthSegments + 1) + 2;
                constraints[cIdx].indices = glm::ivec4(p1, p2, 1, 0);
                constraints[cIdx].params = glm::vec2(glm::length(glm::vec3(segmentLength*2, 0, segmentLength*2)), 0.4f);
                cIdx++;
                
                p1 = y * (widthSegments + 1) + x + 2;
                p2 = p1 + 2 * (widthSegments + 1) - 2;
                constraints[cIdx].indices = glm::ivec4(p1, p2, 1, 0);
                constraints[cIdx].params = glm::vec2(glm::length(glm::vec3(segmentLength*2, 0, segmentLength*2)), 0.4f);
                cIdx++;
            }
        }
    }

    // Bend constraints (DOUBLE: add both skip-1 and skip-2)
    // Skip-1 bend (horizontal)
    for (int y = 0; y <= heightSegments; y++) {
        for (int x = 0; x < widthSegments - 1; x++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + 2;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 2, 0);
            constraints[cIdx].params = glm::vec2(segmentLength * 2.0f, 0.2f);  // Reduced from 0.3f
            cIdx++;
        }
    }

    // Skip-1 bend (vertical)
    for (int x = 0; x <= widthSegments; x++) {
        for (int y = 0; y < heightSegments - 1; y++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + 2 * (widthSegments + 1);
            constraints[cIdx].indices = glm::ivec4(p1, p2, 2, 0);
            constraints[cIdx].params = glm::vec2(segmentLength * 2.0f, 0.2f);  // Reduced from 0.3f
            cIdx++;
        }
    }
    
    // Skip-2 bend (horizontal) - extra stability
    for (int y = 0; y <= heightSegments; y++) {
        for (int x = 0; x < widthSegments - 2; x++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + 3;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 2, 0);
            constraints[cIdx].params = glm::vec2(segmentLength * 3.0f, 0.15f);  // Reduced from 0.2f
            cIdx++;
        }
    }

    // Skip-2 bend (vertical) - extra stability
    for (int x = 0; x <= widthSegments; x++) {
        for (int y = 0; y < heightSegments - 2; y++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + 3 * (widthSegments + 1);
            constraints[cIdx].indices = glm::ivec4(p1, p2, 2, 0);
            constraints[cIdx].params = glm::vec2(segmentLength * 3.0f, 0.15f);  // Reduced from 0.2f
            cIdx++;
        }
    }

    // Upload to GPU - PRE-ALLOCATE large buffer for all 3 cloths
    if (m_TotalParticles == 0) {
        // First cloth - allocate maximum buffer size upfront
        std::vector<ParticleData> particles(numParticles);

        for (int y = 0; y <= heightSegments; y++) {
            for (int x = 0; x <= widthSegments; x++) {
                int idx = y * (widthSegments + 1) + x;
                particles[idx].position = glm::vec4(
                    startX + x * segmentLength,
                    startY,
                    startZ + y * segmentLength,
                    1.0f  // mass
                );
                particles[idx].prevPosition = particles[idx].position;
                particles[idx].velocity = glm::vec4(0.0f, 0.0f, 0.0f, pinned ? 1.0f : 0.0f);
            }
        }

        // Allocate MAX buffer size upfront
        m_ParticleBuffer.Initialize(MAX_PARTICLES);
        m_ParticleBuffer.UploadSubset(0, particles.size(), particles);

        // Create pinned flags buffer with MAX size
        std::vector<float> pinnedFlags(MAX_PARTICLES, 1.0f);  // All pinned by default
        m_PinnedFlagsCPU.resize(MAX_PARTICLES, 1.0f);
        glGenBuffers(1, &m_PinnedFlagsBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, pinnedFlags.size() * sizeof(float),
                     pinnedFlags.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Initialize particle colors for graph coloring (9-color pattern)
        std::vector<unsigned int> particleColors(MAX_PARTICLES, 0);
        for (int y = 0; y <= heightSegments; y++) {
            for (int x = 0; x <= widthSegments; x++) {
                int idx = y * (widthSegments + 1) + x;
                particleColors[idx] = ((x % 3) + (y % 3) * 3);
            }
        }
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleColorsBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, particleColors.size() * sizeof(unsigned int),
                     particleColors.data(), GL_DYNAMIC_COPY);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Allocate MAX constraint buffer size
        m_ConstraintBuffer.Initialize(MAX_CONSTRAINTS);
        m_ConstraintBuffer.UploadSubset(0, constraints.size(), constraints);

        m_TotalParticles = numParticles;  // No dummy particle
        m_TotalConstraints = numConstraints;

        // Return offset 0 (no dummy particle)
        return 0;
    } else {
        // Subsequent cloths - append to existing buffer
        size_t newTotalParticles = m_TotalParticles + numParticles;
        size_t newTotalConstraints = m_TotalConstraints + numConstraints;

        // Upload new particles at offset
        m_ParticleBuffer.UploadSubset(m_TotalParticles, numParticles, particles);

        // Update pinned flags for new particles (they start pinned)
        for (size_t i = m_TotalParticles; i < newTotalParticles; i++) {
            m_PinnedFlagsCPU[i] = 1.0f;
        }
        // Upload updated pinned flags
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, m_PinnedFlagsCPU.size() * sizeof(float),
                        m_PinnedFlagsCPU.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // CRITICAL: Offset particle indices in constraints for this cloth
        std::vector<ConstraintData> offsetConstraints = constraints;
        for (size_t c = 0; c < offsetConstraints.size(); c++) {
            offsetConstraints[c].indices.x += (int)m_TotalParticles;  // p1 offset
            offsetConstraints[c].indices.y += (int)m_TotalParticles;  // p2 offset
        }
        
        // Upload new constraints at offset
        m_ConstraintBuffer.UploadSubset(m_TotalConstraints, numConstraints, offsetConstraints);

        // Initialize particle colors for NEW cloth at offset (9-color pattern)
        std::vector<unsigned int> newClothColors(numParticles);
        for (int y = 0; y <= heightSegments; y++) {
            for (int x = 0; x <= widthSegments; x++) {
                int idx = y * (widthSegments + 1) + x;
                newClothColors[idx] = ((x % 3) + (y % 3) * 3);
            }
        }
        // Upload colors at correct offset
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ParticleColorsBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        m_TotalParticles * sizeof(unsigned int),  // Offset for this cloth
                        newClothColors.size() * sizeof(unsigned int),
                        newClothColors.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        m_TotalParticles = newTotalParticles;
        m_TotalConstraints = newTotalConstraints;
    }

    // Build constraint adjacency list for optimized constraint solving
    BuildConstraintAdjacencyList();

    return particleOffset;
}

void GPUPhysicsWorld::SetParticlesPinned(size_t startIdx, size_t count, bool pinned) {
    // Use CPU copy directly (no GPU read-back needed)
    if (m_PinnedFlagsCPU.empty() || m_PinnedFlagsCPU.size() < m_TotalParticles) {
        std::cerr << "[SetParticlesPinned] ERROR: CPU flags buffer not initialized!" << std::endl;
        return;
    }

    // Update flags for this cloth
    float flagValue = pinned ? 1.0f : 0.0f;
    for (size_t i = startIdx; i < startIdx + count; i++) {
        m_PinnedFlagsCPU[i] = flagValue;
    }

    // Upload ALL updated flags to GPU buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    0,
                    m_TotalParticles * sizeof(float),
                    m_PinnedFlagsCPU.data());
    
    // CRITICAL: Re-bind to binding point 4 for compute shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_PinnedFlagsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Memory barrier to ensure compute shader sees the update
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    glFlush();
}

void GPUPhysicsWorld::Update(float deltaTime) {
    if (!m_Initialized || !m_ShaderLoaded) {
        return;
    }

    // Clamp deltaTime
    if (deltaTime > 0.033f) deltaTime = 0.033f;
    if (deltaTime < 0.001f) deltaTime = 0.001f;

    // Update uniforms with current deltaTime
    UpdateUniforms(deltaTime);

    // Bind compute shader
    m_ComputeShader.Bind();

    // Check shader program is valid
    if (m_ComputeShader.GetID() == 0) {
        std::cerr << "[GPUPhysicsWorld] ERROR: Compute shader ID is 0!" << std::endl;
        return;
    }

    // Validate shader program once at startup
    if (!m_ShaderValidated) {
        GLint success;
        glGetProgramiv(m_ComputeShader.GetID(), GL_VALIDATE_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetProgramInfoLog(m_ComputeShader.GetID(), 1024, NULL, infoLog);
            std::cerr << "[GPUPhysicsWorld] ERROR: Compute shader validation failed!" << std::endl;
            std::cerr << "[GPUPhysicsWorld] Info log: " << (infoLog[0] != '\0' ? infoLog : "(empty)") << std::endl;
            return;
        }
        m_ShaderValidated = true;
    }

    // Clear any pending errors
    while (glGetError() != GL_NO_ERROR);

    // Bind terrain heightmap texture
    glActiveTexture(GL_TEXTURE0);
    if (m_TerrainHeightmapTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, m_TerrainHeightmapTexture);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    m_ComputeShader.SetInt("terrainHeightmap", 0);

    // Clear errors after texture bind
    while (glGetError() != GL_NO_ERROR);

    // Bind SSBOs to binding points 0, 1, 4, 5, 6, 7, 8
    m_ParticleBuffer.Bind();  // Binding point 0
    m_ConstraintBuffer.Bind();  // Binding point 1

    // Bind pinned flags buffer to binding point 4
    if (m_PinnedFlagsBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_PinnedFlagsBuffer);
    }
    
    // Bind atomic collision buffers
    if (m_CollisionCountBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_CollisionCountBuffer);
    }
    if (m_CollisionDataBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, m_CollisionDataBuffer);
    }

    // Bind constraint adjacency list buffer (binding point 9)
    if (m_ConstraintAdjacencyBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, m_ConstraintAdjacencyBuffer);
    }

    // Bind constraint indices buffer (binding point 10)
    if (m_ConstraintIndicesBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, m_ConstraintIndicesBuffer);
    }

    // Bind graph coloring buffers
    if (m_ParticleColorsBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_ParticleColorsBuffer);
    }
    if (m_ColorCollisionCountBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, m_ColorCollisionCountBuffer);
    }

    // Bind uniform buffers to binding points 2 and 3
    if (m_PhysicsParamsBlockIndex != -1) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_UniformBuffer);
    }
    if (m_CollisionParamsBlockIndex != -1) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_CollisionUniformBuffer);
    }

    // Reset collision count at start of frame
    unsigned int zeroCount = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_CollisionCountBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(unsigned int), &zeroCount);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // CRITICAL: Memory barrier BEFORE dispatch to ensure compute shader sees
    // the latest particle data (especially velocity.w for pinned state)
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_UNIFORM_BARRIER_BIT);

    // Calculate work groups
    unsigned int totalWorkGroups = static_cast<unsigned int>((m_TotalParticles + m_WorkGroupSize - 1) / m_WorkGroupSize);

    // Safety check: ensure we have valid work groups
    if (totalWorkGroups == 0 || m_TotalParticles == 0) {
        std::cerr << "[GPUPhysicsWorld] No particles to simulate!" << std::endl;
        m_ParticleBuffer.Unbind();
        m_ConstraintBuffer.Unbind();
        m_ComputeShader.Unbind();
        return;
    }

    // === DISPATCH COMPUTE SHADER ===
    glDispatchCompute(totalWorkGroups, 1, 1);

    // Check for dispatch errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[GPUPhysicsWorld] glDispatchCompute ERROR: " << err << std::endl;
        m_ParticleBuffer.Unbind();
        m_ConstraintBuffer.Unbind();
        m_ComputeShader.Unbind();
        return;
    }

    // Memory barrier to ensure rendering sees updated data
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Wait for GPU completion to ensure physics sync
    glFinish();

    // Swap double buffers for next frame
    if (m_ParticleBuffer.IsDoubleBuffered()) {
        m_ParticleBuffer.SwapBuffers();
    }

    // Unbind
    m_ParticleBuffer.Unbind();
    m_ConstraintBuffer.Unbind();
    m_ComputeShader.Unbind();
}

void GPUPhysicsWorld::UpdateUniforms(float deltaTime) {
    // Physics params uniform block
    // Layout must match shader exactly (std140 layout):
    // - vec3 gravity (16 bytes: xyz + padding)
    // - float deltaTime (4 bytes)
    // - float damping (4 bytes)
    // - int numParticles (4 bytes)
    // - int numConstraints (4 bytes)
    // - float restLength (4 bytes)
    // - vec2 terrainSize (8 bytes)
    // - float terrainHeightScale (4 bytes)
    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);

    float physicsData[16]; // 4 vec4s = 64 bytes
    // vec4: gravity
    physicsData[0] = m_Config.gravity.x;
    physicsData[1] = m_Config.gravity.y;
    physicsData[2] = m_Config.gravity.z;
    physicsData[3] = deltaTime;  // deltaTime comes AFTER gravity (matches shader layout)

    // vec4: damping, numParticles, numConstraints, restLength
    physicsData[4] = m_Config.damping;
    physicsData[5] = static_cast<float>(m_TotalParticles);
    physicsData[6] = static_cast<float>(m_TotalConstraints);
    physicsData[7] = 0.0f;  // restLength (unused, padding)

    // vec4: terrainSize (vec2) + terrainHeightScale + padding
    if (m_Terrain) {
        physicsData[8] = 100.0f; // terrainSize.x
        physicsData[9] = 100.0f; // terrainSize.y
        physicsData[10] = 1.0f;  // terrainHeightScale
    } else {
        physicsData[8] = 100.0f;
        physicsData[9] = 100.0f;
        physicsData[10] = 1.0f;
    }
    physicsData[11] = 0.0f;  // padding

    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(physicsData), physicsData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Bind uniform buffer to binding point 2 (NOT 0 - SSBOs use 0 and 1)
    if (m_PhysicsParamsBlockIndex != -1) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_UniformBuffer);
    }

    // Collision params
    glBindBuffer(GL_UNIFORM_BUFFER, m_CollisionUniformBuffer);

    float collisionData[12]; // 3 vec4s = 48 bytes
    collisionData[0] = m_Collision.sphereCenter.x;
    collisionData[1] = m_Collision.sphereCenter.y;
    collisionData[2] = m_Collision.sphereCenter.z;
    collisionData[3] = m_Collision.sphereRadius;

    collisionData[4] = m_Collision.groundLevel;
    collisionData[5] = m_Config.collisionMargin;
    collisionData[6] = m_Config.dampingFactor;
    collisionData[7] = m_Config.frictionFactor;
    collisionData[8] = static_cast<float>(m_Config.collisionSubsteps);

    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(collisionData), collisionData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Bind collision uniform buffer to binding point 3 (NOT 1 - SSBOs use 0 and 1)
    if (m_CollisionParamsBlockIndex != -1) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_CollisionUniformBuffer);
    }
}

void GPUPhysicsWorld::SetCollisionSphere(const glm::vec3& center, float radius) {
    m_Collision.sphereCenter = center;
    m_Collision.sphereRadius = radius;
    m_Collision.hasSphere = true;
    UpdateUniforms(1.0f / 60.0f);
}

void GPUPhysicsWorld::BuildConstraintAdjacencyList() {
    // Download ONLY initialized constraints from GPU (not entire MAX_CONSTRAINTS buffer)
    std::vector<ConstraintData> allConstraints = m_ConstraintBuffer.DownloadData();

    // Only process constraints up to m_TotalConstraints (ignore uninitialized data)
    std::vector<ConstraintData> constraints;
    constraints.reserve(m_TotalConstraints);
    for (size_t i = 0; i < std::min(allConstraints.size(), m_TotalConstraints); i++) {
        constraints.push_back(allConstraints[i]);
    }

    std::cout << "[GPU] Building adjacency list: " << m_TotalParticles << " particles, "
              << m_TotalConstraints << " constraints (from " << allConstraints.size() << " buffer)" << std::endl;

    // CRITICAL: Check for constraint buffer overflow
    if (m_TotalConstraints > MAX_CONSTRAINTS) {
        std::cerr << "[GPU] ERROR: Total constraints (" << m_TotalConstraints 
                  << ") exceeds MAX_CONSTRAINTS (" << MAX_CONSTRAINTS << ")!" << std::endl;
        std::cerr << "[GPU] This will cause buffer overflow and simulation instability!" << std::endl;
    }

    std::vector<std::vector<int>> particleConstraints(m_TotalParticles);

    // First pass: count constraints per particle with validation
    int invalidConstraints = 0;
    for (size_t c = 0; c < constraints.size(); c++) {
        const auto& constraint = constraints[c];
        int p1 = constraint.indices.x;
        int p2 = constraint.indices.y;

        // Validate particle indices (catch corrupted data from buffer overflow)
        if (p1 < 0 || p1 >= (int)m_TotalParticles || p2 < 0 || p2 >= (int)m_TotalParticles) {
            invalidConstraints++;
            if (invalidConstraints <= 5) {
                std::cerr << "[GPU] WARNING: Invalid constraint " << c << " indices: p1=" << p1 
                          << ", p2=" << p2 << " (total particles=" << m_TotalParticles << ")" << std::endl;
            }
            continue;
        }

        particleConstraints[p1].push_back(static_cast<int>(c));
        particleConstraints[p2].push_back(static_cast<int>(c));
    }

    if (invalidConstraints > 0) {
        std::cerr << "[GPU] WARNING: " << invalidConstraints << " invalid constraints found (buffer overflow?)" << std::endl;
    }

    // Build flat constraint index array and adjacency entries
    std::vector<int> constraintIndices;
    std::vector<glm::ivec2> adjacency(m_TotalParticles);  // (offset, count)

    int maxConstraintsPerParticle = 0;
    int totalConstraintRefs = 0;

    for (size_t p = 0; p < m_TotalParticles; p++) {
        adjacency[p].x = static_cast<int>(constraintIndices.size());  // offset
        adjacency[p].y = static_cast<int>(particleConstraints[p].size());  // count

        // Sanity check: with double constraints, particles can have 20-24 constraints
        if (adjacency[p].y > 24) {
            std::cerr << "[GPU] WARNING: Particle " << p << " has " << adjacency[p].y
                      << " constraints - possible corruption!" << std::endl;
        }

        maxConstraintsPerParticle = std::max(maxConstraintsPerParticle, adjacency[p].y);
        totalConstraintRefs += adjacency[p].y;

        // Add constraint indices for this particle
        for (int cIdx : particleConstraints[p]) {
            constraintIndices.push_back(cIdx);
        }
    }

    std::cout << "[GPU] Adjacency stats: max constraints/particle=" << maxConstraintsPerParticle
              << ", avg=" << (float)totalConstraintRefs / m_TotalParticles
              << ", total refs=" << totalConstraintRefs << std::endl;

    // Upload adjacency list to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintAdjacencyBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    m_TotalParticles * sizeof(glm::ivec2),
                    adjacency.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Upload constraint indices to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_ConstraintIndicesBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0,
                    constraintIndices.size() * sizeof(int),
                    constraintIndices.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    std::cout << "[GPU] ✓ Built constraint adjacency list" << std::endl;
}

void GPUPhysicsWorld::CreateTerrainHeightmapTexture() {
    if (!m_Terrain) return;

    // Generate heightmap texture from terrain
    const int textureSize = 256;
    std::vector<float> heightData(textureSize * textureSize);

    // Sample terrain height at each texel
    for (int y = 0; y < textureSize; y++) {
        for (int x = 0; x < textureSize; x++) {
            float u = static_cast<float>(x) / textureSize;
            float v = static_cast<float>(y) / textureSize;

            // Convert to world coordinates
            float worldX = (u - 0.5f) * 100.0f; // Assuming 100x100 terrain
            float worldZ = (v - 0.5f) * 100.0f;

            float height = m_Terrain->GetHeightAt(worldX, worldZ);

            // Normalize to [0, 1] range
            heightData[y * textureSize + x] = height * 0.5f + 0.5f;
        }
    }

    // Create or update texture
    if (m_TerrainHeightmapTexture == 0) {
        glGenTextures(1, &m_TerrainHeightmapTexture);
    }

    glBindTexture(GL_TEXTURE_2D, m_TerrainHeightmapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, textureSize, textureSize, 0, GL_RED, GL_FLOAT, heightData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GPUPhysicsWorld::BindForRendering() const {
    m_ParticleBuffer.Bind();
}

void GPUPhysicsWorld::Unbind() const {
    m_ParticleBuffer.Unbind();
    m_ConstraintBuffer.Unbind();
}

std::vector<ParticleData> GPUPhysicsWorld::DownloadParticles() const {
    return m_ParticleBuffer.DownloadData();
}

// ============================================================================
// AUTO-TUNE IMPLEMENTATION
// ============================================================================

void GPUPhysicsWorld::AutoTuneSettings() {
    std::cout << "\n[Auto-Tune] Running GPU benchmark..." << std::endl;
    
    // Run micro-benchmark: dispatch compute shader and measure time
    // IMPORTANT: Must sync after EACH dispatch to measure real GPU time
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Simulate 10 frames with 1000 particles
    const int testParticles = 1000;
    const int testIterations = 5;
    const int testFrames = 10;
    
    for (int frame = 0; frame < testFrames; frame++) {
        // Simulate compute dispatch
        unsigned int workGroups = (testParticles + 127) / 128;
        glDispatchCompute(workGroups, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        // CRITICAL: Must sync after EACH dispatch to measure real GPU time
        glFinish();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Calculate FPS (handle division by zero)
    float fps;
    if (duration.count() == 0) {
        // If duration is 0, GPU is VERY fast - use conservative estimate
        fps = 200.0f;
        std::cout << "[Auto-Tune] GPU very fast! Using estimated FPS: 200" << std::endl;
    } else {
        fps = (testFrames * 1000.0f) / duration.count();
    }
    
    std::cout << "[Auto-Tune] Benchmark: " << fps << " FPS (" << testParticles << " particles, " 
              << testIterations << " iterations, " << duration.count() << "ms)" << std::endl;
    
    // Determine optimal settings based on FPS
    if (fps > 120) {
        m_MaxIterations = 8;  // Reduced from 10 for stability
        m_UseTextureGather = true;
        std::cout << "[Auto-Tune] Setting: ULTRA (8 iterations, textureGather)" << std::endl;
    } else if (fps > 60) {
        m_MaxIterations = 5;
        m_UseTextureGather = true;
        std::cout << "[Auto-Tune] Setting: HIGH (5 iterations, textureGather)" << std::endl;
    } else if (fps > 30) {
        m_MaxIterations = 3;
        m_UseTextureGather = false;
        std::cout << "[Auto-Tune] Setting: MEDIUM (3 iterations, textureLod)" << std::endl;
    } else {
        m_MaxIterations = 2;
        m_UseTextureGather = false;
        std::cout << "[Auto-Tune] Setting: LOW (2 iterations, textureLod)" << std::endl;
    }
    
    // Recompile shader with optimal settings
    std::cout << "[Auto-Tune] Recompiling shader with optimal settings..." << std::endl;
    LoadComputeShader();  // This will use the new m_MaxIterations and m_UseTextureGather
}

void GPUPhysicsWorld::SetQualityLevel(int iterations, bool useTextureGather) {
    m_MaxIterations = iterations;
    m_UseTextureGather = useTextureGather;
    m_ManualPresetSelected = true;  // Mark as manual selection
    m_AutoTuningEnabled = false;    // Disable auto-tune
    LoadComputeShader();  // Recompile with new settings
}

} // namespace cloth
