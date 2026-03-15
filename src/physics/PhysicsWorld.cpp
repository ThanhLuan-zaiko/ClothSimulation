#include "PhysicsWorld.h"
#include "world/Terrain.h"
#include <iostream>

namespace cloth {

PhysicsWorld::PhysicsWorld()
    : m_Gravity(0.0f, -9.81f, 0.0f),
      m_Iterations(5),
      m_TimeStep(1.0f / 60.0f) {
}

PhysicsWorld::~PhysicsWorld() {
    Clear();
}

void PhysicsWorld::Update(float deltaTime) {
    // Clamp deltaTime to prevent physics explosion
    // Max 1/30s = 0.033s to prevent huge jumps
    if (deltaTime > 0.033f) {
        deltaTime = 0.033f;
    }
    if (deltaTime < 0.001f) {
        deltaTime = 0.001f;
    }
    
    // Apply gravity to all particles (skip particles with mass <= 0 or pinned)
    int activeCount = 0;
    for (Particle* particle : m_Particles) {
        if (!particle->isPinned && particle->mass > 0.0f) {
            activeCount++;
            particle->ApplyForce(m_Gravity * particle->mass);

            // Calculate acceleration: a = F / m
            particle->acceleration = particle->force / particle->mass;
        }
    }
    
    // Debug: Show collision setup on first frame
    static bool firstUpdate = true;
    if (firstUpdate) {
        std::cout << "[PHYSICS] Collision setup - Sphere: r=" << m_SphereRadius 
                  << " at y=" << m_SphereCenter.y << ", Ground: y=" << m_GroundLevel
                  << ", Terrain: " << (m_Terrain ? "ENABLED" : "DISABLED") << std::endl;
        firstUpdate = false;
    }

    // Update particles (Verlet integration)
    for (Particle* particle : m_Particles) {
        particle->Update(deltaTime);
        particle->ResetForce();

        if (!particle->isPinned) {
            // === Option A: Ground plane collision ===
            if (particle->position.y < m_GroundLevel) {
                particle->position.y = m_GroundLevel;
                particle->previousPos.y = m_GroundLevel; // Stop vertical velocity
            }
            
            // === Option C: Terrain heightmap collision ===
            if (m_Terrain != nullptr) {
                float terrainHeight = m_Terrain->GetHeightAt(particle->position.x, particle->position.z);
                if (particle->position.y < terrainHeight) {
                    particle->position.y = terrainHeight;
                    particle->previousPos.y = terrainHeight; // Stop vertical velocity
                }
            }

            // === Option B: Improved sphere collision ===
            if (m_HasSphere) {
                glm::vec3 diff = particle->position - m_SphereCenter;
                float dist = glm::length(diff);
                float minRadius = m_SphereRadius + 0.02f; // Slight offset for better visuals

                if (dist < minRadius) {
                    glm::vec3 normal = diff / dist;
                    // Push particle out of sphere
                    particle->position = m_SphereCenter + normal * minRadius;

                    // Reflect velocity off sphere surface (bounce effect)
                    glm::vec3 velocity = particle->position - particle->previousPos;
                    float dotVelNormal = glm::dot(velocity, normal);
                    
                    if (dotVelNormal < 0) {
                        // Only reflect if moving toward sphere center
                        // bounceFactor = 0.3 means 30% energy retained in bounce
                        float bounceFactor = 0.3f;
                        velocity -= (1.0f + bounceFactor) * dotVelNormal * normal;
                        
                        // Apply damping to simulate friction
                        float damping = 0.9f;
                        particle->previousPos = particle->position - velocity * damping;
                    }
                }
            }
        }
    }

    // Solve constraints multiple times for stability
    for (int i = 0; i < m_Iterations; i++) {
        for (Constraint* constraint : m_Constraints) {
            constraint->Resolve();
        }
    }
}

Particle* PhysicsWorld::AddParticle(const glm::vec3& position, float mass, bool pinned) {
    Particle* particle = new Particle(position, mass, pinned);
    m_Particles.push_back(particle);
    return particle;
}

void PhysicsWorld::RemoveParticle(Particle* particle) {
    // Remove associated constraints first
    for (auto it = m_Constraints.begin(); it != m_Constraints.end();) {
        if ((*it)->p1 == particle || (*it)->p2 == particle) {
            delete* it;
            it = m_Constraints.erase(it);
        } else {
            ++it;
        }
    }

    // Remove particle
    for (auto it = m_Particles.begin(); it != m_Particles.end(); ++it) {
        if (*it == particle) {
            delete* it;
            m_Particles.erase(it);
            break;
        }
    }
}

Constraint* PhysicsWorld::AddConstraint(Particle* p1, Particle* p2, float stiffness) {
    Constraint* constraint = new Constraint(p1, p2, stiffness);
    m_Constraints.push_back(constraint);
    return constraint;
}

void PhysicsWorld::RemoveConstraint(Constraint* constraint) {
    for (auto it = m_Constraints.begin(); it != m_Constraints.end(); ++it) {
        if (*it == constraint) {
            delete* it;
            m_Constraints.erase(it);
            break;
        }
    }
}

void PhysicsWorld::Clear() {
    for (Particle* particle : m_Particles) {
        delete particle;
    }
    for (Constraint* constraint : m_Constraints) {
        delete constraint;
    }
    m_Particles.clear();
    m_Constraints.clear();
}

} // namespace cloth
