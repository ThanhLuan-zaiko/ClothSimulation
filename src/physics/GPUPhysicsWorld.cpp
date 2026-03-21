#include "GPUPhysicsWorld.h"
#include "utils/GPUDetection.h"
#include "cloth/Cloth.h"
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>  // For benchmark timing
#include <fstream>  // For file I/O
#include <sstream>  // For stringstream

namespace cloth {

GPUPhysicsWorld::GPUPhysicsWorld()
    : m_UniformBuffer(0),
      m_CollisionUniformBuffer(0),
      m_PinnedFlagsBuffer(0),
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
      m_MaxIterations(2),  // Default conservative
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
    int numConstraints = 0;

    // Count constraints
    // Structural (horizontal + vertical)
    numConstraints += widthSegments * (heightSegments + 1);  // Horizontal
    numConstraints += heightSegments * (widthSegments + 1);  // Vertical
    // Shear (diagonal)
    numConstraints += widthSegments * heightSegments * 2;
    // Bend (horizontal + vertical, skip one)
    numConstraints += (widthSegments - 1) * (heightSegments + 1);  // Horizontal bend
    numConstraints += (heightSegments - 1) * (widthSegments + 1);  // Vertical bend

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

    // Shear constraints
    for (int y = 0; y < heightSegments; y++) {
        for (int x = 0; x < widthSegments; x++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + (widthSegments + 1) + 1;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 1, 0);
            constraints[cIdx].params = glm::vec2(glm::length(glm::vec3(segmentLength, 0, segmentLength)), 0.5f);
            cIdx++;

            p1 = y * (widthSegments + 1) + x + 1;
            p2 = p1 + (widthSegments + 1) - 1;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 1, 0);
            constraints[cIdx].params = glm::vec2(glm::length(glm::vec3(segmentLength, 0, segmentLength)), 0.5f);
            cIdx++;
        }
    }

    // Bend constraints
    for (int y = 0; y <= heightSegments; y++) {
        for (int x = 0; x < widthSegments - 1; x++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + 2;
            constraints[cIdx].indices = glm::ivec4(p1, p2, 2, 0);
            constraints[cIdx].params = glm::vec2(segmentLength * 2.0f, 0.1f);
            cIdx++;
        }
    }

    for (int x = 0; x <= widthSegments; x++) {
        for (int y = 0; y < heightSegments - 1; y++) {
            int p1 = y * (widthSegments + 1) + x;
            int p2 = p1 + 2 * (widthSegments + 1);
            constraints[cIdx].indices = glm::ivec4(p1, p2, 2, 0);
            constraints[cIdx].params = glm::vec2(segmentLength * 2.0f, 0.1f);
            cIdx++;
        }
    }

    // Upload to GPU - Use SINGLE BUFFER (double buffering causes sync issues)
    if (m_TotalParticles == 0) {
        // Add 1 dummy particle at index 0 to avoid offset 0 for cloth 1
        std::vector<ParticleData> particlesWithDummy(numParticles + 1);
        
        // Dummy particle at high position (won't interfere with simulation)
        particlesWithDummy[0].position = glm::vec4(0.0f, 10000.0f, 0.0f, 1.0f);
        particlesWithDummy[0].prevPosition = particlesWithDummy[0].position;
        particlesWithDummy[0].velocity = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // pinned
        
        // Copy actual particles starting at index 1
        for (size_t i = 0; i < numParticles; i++) {
            particlesWithDummy[i + 1] = particles[i];
        }
        
        m_ParticleBuffer.Initialize(numParticles + 1);  // Single buffer
        m_ParticleBuffer.UploadData(particlesWithDummy);
        
        // Create pinned flags buffer
        std::vector<float> pinnedFlags(numParticles + 1, 1.0f);  // All pinned by default
        glGenBuffers(1, &m_PinnedFlagsBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, pinnedFlags.size() * sizeof(float), 
                     pinnedFlags.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        m_ConstraintBuffer.Initialize(numConstraints);
        m_ConstraintBuffer.UploadData(constraints);

        m_TotalParticles = numParticles + 1;  // Include dummy particle
        m_TotalConstraints = numConstraints;
        
        // Return offset 1 (skip dummy particle)
        return 1;
    } else {
        // Subsequent cloths - need to resize buffers
        size_t newTotalParticles = m_TotalParticles + numParticles;
        size_t newTotalConstraints = m_TotalConstraints + numConstraints;

        // Download existing particles
        std::vector<ParticleData> existingParticles = m_ParticleBuffer.DownloadData();

        // Create combined particle array
        std::vector<ParticleData> combinedParticles(newTotalParticles);

        // Copy existing particles WITH THEIR PINNED STATE
        for (size_t i = 0; i < m_TotalParticles && i < existingParticles.size(); i++) {
            combinedParticles[i] = existingParticles[i];
        }

        // Add new particles at offset
        for (size_t i = 0; i < numParticles; i++) {
            combinedParticles[m_TotalParticles + i] = particles[i];
        }

        // Re-initialize particle buffer with new size
        m_ParticleBuffer.Initialize(newTotalParticles);
        m_ParticleBuffer.UploadData(combinedParticles);
        
        // Resize pinned flags buffer
        std::vector<float> existingFlags(m_TotalParticles, 1.0f);
        if (m_PinnedFlagsBuffer != 0) {
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
            float* flagsPtr = static_cast<float*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 
                                                                   0, 
                                                                   m_TotalParticles * sizeof(float),
                                                                   GL_MAP_READ_BIT));
            if (flagsPtr) {
                memcpy(existingFlags.data(), flagsPtr, m_TotalParticles * sizeof(float));
                glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            }
            glDeleteBuffers(1, &m_PinnedFlagsBuffer);
        }
        
        // Create new pinned flags buffer with expanded size
        std::vector<float> newPinnedFlags(newTotalParticles, 1.0f);
        for (size_t i = 0; i < m_TotalParticles; i++) {
            newPinnedFlags[i] = existingFlags[i];
        }
        // New particles are pinned by default (velocity.w = 1.0)
        
        glGenBuffers(1, &m_PinnedFlagsBuffer);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, newPinnedFlags.size() * sizeof(float), 
                     newPinnedFlags.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Download existing constraints
        std::vector<ConstraintData> existingConstraints = m_ConstraintBuffer.DownloadData();

        // Create combined constraint array
        std::vector<ConstraintData> combinedConstraints(newTotalConstraints);

        // Copy existing constraints
        for (size_t i = 0; i < m_TotalConstraints && i < existingConstraints.size(); i++) {
            combinedConstraints[i] = existingConstraints[i];
        }

        // Add new constraints at offset
        for (size_t i = 0; i < numConstraints; i++) {
            combinedConstraints[m_TotalConstraints + i] = constraints[i];
        }

        // Re-initialize constraint buffer with new size
        m_ConstraintBuffer.Initialize(newTotalConstraints);
        m_ConstraintBuffer.UploadData(combinedConstraints);

        m_TotalParticles = newTotalParticles;
        m_TotalConstraints = newTotalConstraints;
    }

    return particleOffset;
}

void GPUPhysicsWorld::SetParticlesPinned(size_t startIdx, size_t count, bool pinned) {
    // Update pinned flags buffer directly
    std::vector<float> pinnedFlags(startIdx + count, 1.0f);
    
    // Download existing flags
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
    float* flagsPtr = static_cast<float*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 
                                                           0, 
                                                           pinnedFlags.size() * sizeof(float),
                                                           GL_MAP_READ_BIT | GL_MAP_WRITE_BIT));
    if (flagsPtr) {
        memcpy(pinnedFlags.data(), flagsPtr, pinnedFlags.size() * sizeof(float));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    
    // Update flags for this cloth
    float flagValue = pinned ? 1.0f : 0.0f;
    for (size_t i = startIdx; i < startIdx + count; i++) {
        pinnedFlags[i] = flagValue;
    }
    
    // Upload updated flags
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_PinnedFlagsBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 
                    startIdx * sizeof(float),
                    count * sizeof(float),
                    pinnedFlags.data() + startIdx);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Memory barrier to ensure compute shader sees the update
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
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

    // Bind SSBOs to binding points 0, 1, and 4
    m_ParticleBuffer.Bind();  // Binding point 0
    m_ConstraintBuffer.Bind();  // Binding point 1
    
    // Bind pinned flags buffer to binding point 4
    if (m_PinnedFlagsBuffer != 0) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_PinnedFlagsBuffer);
    }

    // Bind uniform buffers to binding points 2 and 3
    if (m_PhysicsParamsBlockIndex != -1) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_UniformBuffer);
    }
    if (m_CollisionParamsBlockIndex != -1) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_CollisionUniformBuffer);
    }
    
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
