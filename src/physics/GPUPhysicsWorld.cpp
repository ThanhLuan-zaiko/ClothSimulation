#include "GPUPhysicsWorld.h"
#include "cloth/Cloth.h"
#include <glad/glad.h>
#include <iostream>
#include <vector>
#include <cmath>

namespace cloth {

GPUPhysicsWorld::GPUPhysicsWorld()
    : m_ShaderLoaded(false),
      m_Terrain(nullptr),
      m_TerrainHeightmapTexture(0),
      m_WorkGroupSize(256),
      m_TotalParticles(0),
      m_TotalConstraints(0),
      m_Initialized(false) {
}

GPUPhysicsWorld::~GPUPhysicsWorld() {
    if (m_UniformBuffer != 0) {
        glDeleteBuffers(1, &m_UniformBuffer);
    }
    if (m_CollisionUniformBuffer != 0) {
        glDeleteBuffers(1, &m_CollisionUniformBuffer);
    }
    if (m_TerrainHeightmapTexture != 0) {
        glDeleteTextures(1, &m_TerrainHeightmapTexture);
    }
}

bool GPUPhysicsWorld::Initialize(const GPUPhysicsConfig& config) {
    m_Config = config;

    // Load compute shader
    if (!LoadComputeShader()) {
        std::cerr << "[GPUPhysicsWorld] Failed to load compute shader!" << std::endl;
        return false;
    }

    // Create uniform buffers
    glGenBuffers(1, &m_UniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * 4, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glGenBuffers(1, &m_CollisionUniformBuffer);
    glBindBuffer(GL_UNIFORM_BUFFER, m_CollisionUniformBuffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec4) * 3, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    UpdateUniforms(1.0f / 60.0f);

    // IMPORTANT: Set initialized flag
    m_Initialized = true;

    return true;
}

bool GPUPhysicsWorld::LoadComputeShader() {
    // Try to load from file
    std::string shaderPath = "shaders/physics/physics.comp";

    m_ComputeShader = Shader::CreateComputeShaderFromFile(shaderPath);

    if (m_ComputeShader.GetID() != 0) {
        m_ShaderLoaded = true;
        std::cout << "[GPUPhysicsWorld] Compute shader loaded from " << shaderPath << std::endl;
        return true;
    }

    // Try alternate path (relative to executable)
    shaderPath = "../shaders/physics/physics.comp";
    m_ComputeShader = Shader::CreateComputeShaderFromFile(shaderPath);

    if (m_ComputeShader.GetID() != 0) {
        m_ShaderLoaded = true;
        std::cout << "[GPUPhysicsWorld] Compute shader loaded from " << shaderPath << std::endl;
        return true;
    }

    std::cerr << "[GPUPhysicsWorld] Failed to load compute shader from file!" << std::endl;
    return false;
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

    // Upload to GPU - Support multiple cloths with buffer resize
    if (m_TotalParticles == 0) {
        // First cloth - initialize buffers
        m_ParticleBuffer.Initialize(numParticles);
        m_ParticleBuffer.UploadData(particles);

        m_ConstraintBuffer.Initialize(numConstraints);
        m_ConstraintBuffer.UploadData(constraints);

        m_TotalParticles = numParticles;
        m_TotalConstraints = numConstraints;
    } else {
        // Subsequent cloths - need to resize buffers
        size_t newTotalParticles = m_TotalParticles + numParticles;
        size_t newTotalConstraints = m_TotalConstraints + numConstraints;

        // Download existing particles
        std::vector<ParticleData> existingParticles = m_ParticleBuffer.DownloadData();

        // Create combined particle array
        std::vector<ParticleData> combinedParticles(newTotalParticles);

        // Copy existing particles
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
    // Download particles, update pinned state, and re-upload
    std::vector<ParticleData> particles = m_ParticleBuffer.DownloadData();

    for (size_t i = startIdx; i < startIdx + count && i < particles.size(); i++) {
        particles[i].velocity.w = pinned ? 1.0f : 0.0f;
    }

    m_ParticleBuffer.UploadData(particles);
}

void GPUPhysicsWorld::Update(float deltaTime) {
    // Debug: Log every call
    static int callCount = 0;
    if (callCount < 10) {
        std::cout << "[GPUPhysicsWorld] Update() called #" << callCount 
                  << " (initialized=" << m_Initialized << ", shaderLoaded=" << m_ShaderLoaded << ")" << std::endl;
        callCount++;
    }
    
    if (!m_Initialized || !m_ShaderLoaded) {
        std::cout << "[GPUPhysicsWorld] Update() EARLY RETURN: initialized=" << m_Initialized 
                  << ", shaderLoaded=" << m_ShaderLoaded << std::endl;
        return;
    }

    // Clamp deltaTime
    if (deltaTime > 0.033f) deltaTime = 0.033f;
    if (deltaTime < 0.001f) deltaTime = 0.001f;

    // Update uniforms with current deltaTime
    UpdateUniforms(deltaTime);

    // Bind compute shader
    m_ComputeShader.Bind();

    // Bind terrain heightmap texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TerrainHeightmapTexture);
    m_ComputeShader.SetInt("terrainHeightmap", 0);

    // Bind SSBOs
    m_ParticleBuffer.Bind();
    m_ConstraintBuffer.Bind();

    // Dispatch compute shader
    unsigned int numWorkGroups = static_cast<unsigned int>((m_TotalParticles + m_WorkGroupSize - 1) / m_WorkGroupSize);
    
    // Debug: Log every frame for first 100 frames
    static int updateCount = 0;
    if (updateCount < 100) {
        std::cout << "[GPUPhysicsWorld] Update #" << updateCount 
                  << ": dispatching " << numWorkGroups << " work groups for " 
                  << m_TotalParticles << " particles, dt=" << deltaTime << std::endl;
        updateCount++;
    }
    
    glDispatchCompute(numWorkGroups, 1, 1);

    // Check for dispatch errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[GPUPhysicsWorld] glDispatchCompute ERROR: " << err << std::endl;
    }

    // Memory barrier to ensure rendering sees updated data
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);

    // Check for barrier errors
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "[GPUPhysicsWorld] glMemoryBarrier ERROR: " << err << std::endl;
    }

    // Unbind
    m_ParticleBuffer.Unbind();
    m_ConstraintBuffer.Unbind();
    m_ComputeShader.Unbind();
}

void GPUPhysicsWorld::UpdateUniforms(float deltaTime) {
    // Physics params uniform block
    glBindBuffer(GL_UNIFORM_BUFFER, m_UniformBuffer);

    float physicsData[16]; // 4 vec4s
    physicsData[0] = m_Config.gravity.x;
    physicsData[1] = m_Config.gravity.y;
    physicsData[2] = m_Config.gravity.z;
    physicsData[3] = m_Config.damping;

    physicsData[4] = deltaTime;
    physicsData[5] = static_cast<float>(m_Config.iterations);
    physicsData[6] = static_cast<float>(m_TotalParticles);
    physicsData[7] = static_cast<float>(m_TotalConstraints);

    // terrainSize and terrainHeightScale
    if (m_Terrain) {
        // Get terrain size (assuming 100x100)
        physicsData[8] = 100.0f; // terrainSize.x
        physicsData[9] = 100.0f; // terrainSize.y
        physicsData[10] = 1.0f;  // terrainHeightScale
    } else {
        physicsData[8] = 100.0f;
        physicsData[9] = 100.0f;
        physicsData[10] = 1.0f;
    }

    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(physicsData), physicsData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Bind uniform buffer to compute shader
    unsigned int uniformBlockIndex = glGetUniformBlockIndex(m_ComputeShader.GetID(), "PhysicsParams");
    if (uniformBlockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_ComputeShader.GetID(), uniformBlockIndex, 0);
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_UniformBuffer);
    }

    // Collision params
    glBindBuffer(GL_UNIFORM_BUFFER, m_CollisionUniformBuffer);

    float collisionData[12]; // 3 vec4s
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

    uniformBlockIndex = glGetUniformBlockIndex(m_ComputeShader.GetID(), "CollisionParams");
    if (uniformBlockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_ComputeShader.GetID(), uniformBlockIndex, 1);
        glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_CollisionUniformBuffer);
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

    std::cout << "[GPUPhysicsWorld] Created terrain heightmap texture (" << textureSize << "x" << textureSize << ")" << std::endl;
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

} // namespace cloth
