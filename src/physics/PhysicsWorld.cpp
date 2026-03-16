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

            // === Option C: Enhanced Terrain heightmap collision ===
            if (m_Terrain != nullptr) {
                // Collision parameters
                const float collisionMargin = 0.12f;  // Small offset to prevent sinking
                const float dampingFactor = 0.85f;    // Velocity damping on contact (0 = full stop, 1 = no damping)
                const float frictionFactor = 0.92f;   // Tangential friction (0 = full friction, 1 = no friction)
                const int collisionSubsteps = 3;      // Sub-steps for continuous collision detection

                // Get terrain properties at particle position
                float terrainHeight = m_Terrain->GetHeightAt(particle->position.x, particle->position.z);
                glm::vec3 terrainNormal = m_Terrain->GetNormalAt(particle->position.x, particle->position.z);

                // Continuous collision detection using sub-steps
                glm::vec3 startPos = particle->previousPos;
                glm::vec3 endPos = particle->position;
                glm::vec3 stepVec = (endPos - startPos) / static_cast<float>(collisionSubsteps);
                
                bool collisionDetected = false;
                glm::vec3 collisionPoint = endPos;
                glm::vec3 collisionNormal = terrainNormal;

                // Check each sub-step for collision
                for (int step = 1; step <= collisionSubsteps; step++) {
                    glm::vec3 checkPos = startPos + stepVec * static_cast<float>(step);
                    float checkTerrainHeight = m_Terrain->GetHeightAt(checkPos.x, checkPos.z);
                    
                    if (checkPos.y < checkTerrainHeight + collisionMargin) {
                        collisionDetected = true;
                        collisionPoint = checkPos;
                        collisionNormal = m_Terrain->GetNormalAt(checkPos.x, checkPos.z);
                        break;
                    }
                }

                // Apply collision response if collision detected
                if (collisionDetected || particle->position.y < terrainHeight + collisionMargin) {
                    float targetHeight = terrainHeight + collisionMargin;
                    
                    // Project particle position onto terrain surface
                    glm::vec3 surfacePoint = particle->position;
                    surfacePoint.y = targetHeight;

                    // Calculate velocity before collision
                    glm::vec3 velocity = particle->position - particle->previousPos;

                    // Decompose velocity into normal and tangential components
                    float normalVel = glm::dot(velocity, collisionNormal);
                    glm::vec3 normalComponent = normalVel * collisionNormal;
                    glm::vec3 tangentialComponent = velocity - normalComponent;

                    // Apply damping to normal velocity (energy loss on impact)
                    float dampedNormalVel = normalVel * dampingFactor;
                    
                    // Only apply damping if moving toward terrain (not away)
                    if (normalVel < 0) {
                        // Apply friction to tangential velocity
                        tangentialComponent *= frictionFactor;
                        
                        // Reconstruct velocity with damping and friction
                        velocity = tangentialComponent + dampedNormalVel * collisionNormal;
                        
                        // Update previous position to reflect new velocity
                        particle->previousPos = particle->position - velocity;
                    }

                    // Push particle to terrain surface with margin
                    particle->position.y = std::max(particle->position.y, targetHeight);
                    
                    // Also constrain previous position to prevent tunneling
                    if (particle->previousPos.y < targetHeight) {
                        particle->previousPos.y = targetHeight;
                    }
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
