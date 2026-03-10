#pragma once

#include "../physics/Particle.h"
#include "../physics/Constraint.h"
#include "../physics/PhysicsWorld.h"
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

class Cloth {
public:
    Cloth(PhysicsWorld& world, const ClothConfig& config = ClothConfig());
    ~Cloth();

    void Update(float deltaTime);
    
    // Access particles for rendering
    const std::vector<Particle*>& GetParticles() const { return m_Particles; }
    const std::vector<Constraint*>& GetConstraints() const { return m_Constraints; }

    // Get mesh data
    void GetTriangleVertices(std::vector<glm::vec3>& vertices) const;
    void GetLineVertices(std::vector<glm::vec3>& vertices) const;

    // Accessors
    int GetWidthSegments() const { return m_Config.widthSegments; }
    int GetHeightSegments() const { return m_Config.heightSegments; }

    // Reset cloth to initial position
    void Reset();

private:
    PhysicsWorld& m_World;
    ClothConfig m_Config;
    
    std::vector<Particle*> m_Particles;
    std::vector<Constraint*> m_Constraints;

    void CreateParticles();
    void CreateConstraints();
    Particle* GetParticle(int x, int y);
    const Particle* GetParticle(int x, int y) const;
};

} // namespace cloth
