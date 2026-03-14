#include "Cloth.h"
#include <glm/glm.hpp>

namespace cloth {

Cloth::Cloth(PhysicsWorld& world, const ClothConfig& config)
    : m_World(world), m_Config(config) {
    CreateParticles();
    CreateConstraints();
}

Cloth::~Cloth() {
    // Don't delete particles and constraints - they're owned by PhysicsWorld
    m_Particles.clear();
    m_Constraints.clear();
}

void Cloth::CreateParticles() {
    m_Particles.clear();
    
    for (int y = 0; y <= m_Config.heightSegments; y++) {
        for (int x = 0; x <= m_Config.widthSegments; x++) {
            // Create cloth horizontally in XZ plane
            glm::vec3 pos(
                m_Config.startX + x * m_Config.segmentLength,
                m_Config.startY, // Constant height
                m_Config.startZ + y * m_Config.segmentLength // Expand along Z
            );

            // Pin corners based on config
            bool pinned = false;
            if (m_Config.pinTopLeft && x == 0 && y == 0) {
                pinned = true;
            }
            if (m_Config.pinTopRight && x == m_Config.widthSegments && y == 0) {
                pinned = true;
            }

            Particle* particle = m_World.AddParticle(pos, 1.0f, pinned);
            m_Particles.push_back(particle);
        }
    }
}

void Cloth::CreateConstraints() {
    m_Constraints.clear();

    // Structural constraints (horizontal and vertical)
    for (int y = 0; y <= m_Config.heightSegments; y++) {
        for (int x = 0; x <= m_Config.widthSegments; x++) {
            // Horizontal constraint
            if (x < m_Config.widthSegments) {
                Particle* p1 = GetParticle(x, y);
                Particle* p2 = GetParticle(x + 1, y);
                if (p1 && p2) {
                    m_Constraints.push_back(m_World.AddConstraint(p1, p2, 1.0f));
                }
            }

            // Vertical constraint
            if (y < m_Config.heightSegments) {
                Particle* p1 = GetParticle(x, y);
                Particle* p2 = GetParticle(x, y + 1);
                if (p1 && p2) {
                    m_Constraints.push_back(m_World.AddConstraint(p1, p2, 1.0f));
                }
            }
        }
    }

    // Shear constraints (diagonal)
    for (int y = 0; y < m_Config.heightSegments; y++) {
        for (int x = 0; x < m_Config.widthSegments; x++) {
            Particle* p1 = GetParticle(x, y);
            Particle* p2 = GetParticle(x + 1, y + 1);
            if (p1 && p2) {
                m_Constraints.push_back(m_World.AddConstraint(p1, p2, 0.5f));
            }

            Particle* p3 = GetParticle(x + 1, y);
            Particle* p4 = GetParticle(x, y + 1);
            if (p3 && p4) {
                m_Constraints.push_back(m_World.AddConstraint(p3, p4, 0.5f));
            }
        }
    }

    // Bend constraints (every other particle for flexibility)
    const float bendStiffness = 0.1f;
    
    // Horizontal bend
    for (int y = 0; y <= m_Config.heightSegments; y++) {
        for (int x = 0; x < m_Config.widthSegments - 1; x++) {
            Particle* p1 = GetParticle(x, y);
            Particle* p2 = GetParticle(x + 2, y);
            if (p1 && p2) {
                m_Constraints.push_back(m_World.AddConstraint(p1, p2, bendStiffness));
            }
        }
    }

    // Vertical bend
    for (int x = 0; x <= m_Config.widthSegments; x++) {
        for (int y = 0; y < m_Config.heightSegments - 1; y++) {
            Particle* p1 = GetParticle(x, y);
            Particle* p2 = GetParticle(x, y + 2);
            if (p1 && p2) {
                m_Constraints.push_back(m_World.AddConstraint(p1, p2, bendStiffness));
            }
        }
    }
}

Particle* Cloth::GetParticle(int x, int y) {
    if (x < 0 || x > m_Config.widthSegments || y < 0 || y > m_Config.heightSegments) {
        return nullptr;
    }
    int index = y * (m_Config.widthSegments + 1) + x;
    return m_Particles[index];
}

const Particle* Cloth::GetParticle(int x, int y) const {
    if (x < 0 || x > m_Config.widthSegments || y < 0 || y > m_Config.heightSegments) {
        return nullptr;
    }
    int index = y * (m_Config.widthSegments + 1) + x;
    return m_Particles[index];
}

void Cloth::Update(float deltaTime) {
    // Physics is handled by PhysicsWorld
    // This method can be used for cloth-specific updates
}

void Cloth::GetTriangleVertices(std::vector<glm::vec3>& vertices) const {
    vertices.clear();

    for (int y = 0; y < m_Config.heightSegments; y++) {
        for (int x = 0; x < m_Config.widthSegments; x++) {
            const Particle* p1 = GetParticle(x, y);
            const Particle* p2 = GetParticle(x + 1, y);
            const Particle* p3 = GetParticle(x, y + 1);
            const Particle* p4 = GetParticle(x + 1, y + 1);

            if (p1 && p2 && p3 && p4) {
                // First triangle
                vertices.push_back(p1->position);
                vertices.push_back(p2->position);
                vertices.push_back(p3->position);

                // Second triangle
                vertices.push_back(p2->position);
                vertices.push_back(p4->position);
                vertices.push_back(p3->position);
            }
        }
    }
}

void Cloth::GetLineVertices(std::vector<glm::vec3>& vertices) const {
    vertices.clear();
    
    // Draw constraint lines
    for (const auto* constraint : m_Constraints) {
        if (constraint->p1 && constraint->p2) {
            vertices.push_back(constraint->p1->position);
            vertices.push_back(constraint->p2->position);
        }
    }
}

void Cloth::Reset() {
    m_World.Clear();
    m_Particles.clear();
    m_Constraints.clear();
    CreateParticles();
    CreateConstraints();
}

} // namespace cloth
