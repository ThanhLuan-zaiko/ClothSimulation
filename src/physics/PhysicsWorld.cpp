#include "PhysicsWorld.h"

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
    // Apply gravity to all particles
    for (Particle* particle : m_Particles) {
        if (!particle->isPinned) {
            particle->ApplyForce(m_Gravity * particle->mass);
        }
    }

    // Update particles (Verlet integration)
    for (Particle* particle : m_Particles) {
        particle->Update(deltaTime);
        particle->ResetForce();
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
