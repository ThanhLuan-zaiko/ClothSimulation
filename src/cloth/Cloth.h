#pragma once

#include "../physics/Particle.h"
#include "../physics/Constraint.h"
#include "../physics/GPUPhysicsWorld.h"
#include <vector>
#include <glm/glm.hpp>

namespace cloth {

struct ClothConfig {
    int widthSegments;      // Number of segments along width
    int heightSegments;     // Number of segments along height
    float segmentLength;    // Length of each segment
    float startX;           // Starting X position
    float startY;           // Starting Y position
    float startZ;           // Starting Z position
    bool pinTopLeft;        // Pin the top-left corner
    bool pinTopRight;       // Pin the top-right corner

    ClothConfig()
        : widthSegments(20),
          heightSegments(15),
          segmentLength(0.25f),
          startX(-2.5f),
          startY(3.0f),
          startZ(0.0f),
          pinTopLeft(true),
          pinTopRight(false) {}
};

/**
 * Cloth class - CPU-side representation (for legacy/compatibility)
 * 
 * Note: For GPU physics simulation, cloth data is stored directly on GPU
 * via GPUPhysicsWorld. This class is kept for compatibility and can be
 * used for CPU-based fallback rendering.
 */
class Cloth {
public:
    // Legacy constructor (CPU physics) - deprecated
    Cloth(class PhysicsWorld& world, const ClothConfig& config = ClothConfig());

    // GPU physics constructor (dummy - data is on GPU)
    Cloth(GPUPhysicsWorld& world, const ClothConfig& config = ClothConfig());

    ~Cloth();

    void Update(float deltaTime);

    // Access particles for rendering (returns empty for GPU-based cloth)
    const std::vector<Particle*>& GetParticles() const { return m_Particles; }
    const std::vector<Constraint*>& GetConstraints() const { return m_Constraints; }

    // Get mesh data
    void GetTriangleVertices(std::vector<glm::vec3>& vertices) const;
    void GetLineVertices(std::vector<glm::vec3>& vertices) const;

    // Accessors
    int GetWidthSegments() const { return m_Config.widthSegments; }
    int GetHeightSegments() const { return m_Config.heightSegments; }
    float GetSegmentLength() const { return m_Config.segmentLength; }
    float GetStartX() const { return m_Config.startX; }
    float GetStartY() const { return m_Config.startY; }
    float GetStartZ() const { return m_Config.startZ; }

    // Reset cloth to initial position
    void Reset();

    // Enable/disable gravity for this cloth
    void SetGravityEnabled(bool enabled);
    bool IsGravityEnabled() const;

    // Pin/unpin all particles (to hold cloth in place until drop time)
    void SetAllParticlesPinned(bool pinned);

private:
    ClothConfig m_Config;
    bool m_GravityEnabled = true;  // Control if gravity affects this cloth

    // Empty for GPU-based cloth
    std::vector<Particle*> m_Particles;
    std::vector<Constraint*> m_Constraints;
};

} // namespace cloth
