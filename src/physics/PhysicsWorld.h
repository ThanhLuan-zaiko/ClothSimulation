#pragma once

#include "Particle.h"
#include "Constraint.h"
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace cloth {

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    void Update(float deltaTime);
    void SetGravity(const glm::vec3& gravity) { m_Gravity = gravity; }
    glm::vec3 GetGravity() const { return m_Gravity; }

    // Particle management
    Particle* AddParticle(const glm::vec3& position, float mass = 1.0f, bool pinned = false);
    void RemoveParticle(Particle* particle);
    
    // Constraint management
    Constraint* AddConstraint(Particle* p1, Particle* p2, float stiffness = 1.0f);
    void RemoveConstraint(Constraint* constraint);

    // Accessors
    std::vector<Particle*>& GetParticles() { return m_Particles; }
    std::vector<Constraint*>& GetConstraints() { return m_Constraints; }
    
    const std::vector<Particle*>& GetParticles() const { return m_Particles; }
    const std::vector<Constraint*>& GetConstraints() const { return m_Constraints; }

    // Simulation controls
    void SetIterations(int iterations) { m_Iterations = iterations; }
    int GetIterations() const { return m_Iterations; }

    // Sphere collision
    void SetCollisionSphere(const glm::vec3& center, float radius) {
        m_SphereCenter = center;
        m_SphereRadius = radius;
        m_HasSphere = true;
    }
    
    // Ground collision
    void SetGroundLevel(float y) { m_GroundLevel = y; }
    float GetGroundLevel() const { return m_GroundLevel; }
    
    // Terrain collision (optional - for heightmap-based terrain)
    void SetTerrain(const class Terrain* terrain) { m_Terrain = terrain; }

    void Clear();

private:
    std::vector<Particle*> m_Particles;
    std::vector<Constraint*> m_Constraints;
    
    glm::vec3 m_Gravity;
    int m_Iterations;  // Constraint solver iterations
    float m_TimeStep;

    // Collision objects
    bool m_HasSphere = false;
    glm::vec3 m_SphereCenter = glm::vec3(0.0f);
    float m_SphereRadius = 0.0f;
    
    // Ground plane
    float m_GroundLevel = 0.0f;
    
    // Terrain (for heightmap collision)
    const class Terrain* m_Terrain = nullptr;
};

} // namespace cloth
