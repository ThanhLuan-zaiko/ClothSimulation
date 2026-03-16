#include "Cloth.h"
#include <glm/glm.hpp>

namespace cloth {

// Legacy constructor (CPU physics) - deprecated
Cloth::Cloth(PhysicsWorld& world, const ClothConfig& config)
    : m_Config(config) {
    // Note: This constructor is deprecated, use GPU physics instead
    // Kept for compatibility only
}

// GPU physics constructor (dummy - data is on GPU)
Cloth::Cloth(GPUPhysicsWorld& world, const ClothConfig& config)
    : m_Config(config) {
    // GPU-based cloth - no CPU particles/constraints
    // Data is stored directly on GPU via GPUPhysicsWorld
}

Cloth::~Cloth() {
    m_Particles.clear();
    m_Constraints.clear();
}

void Cloth::Update(float deltaTime) {
    // GPU-based cloth - physics handled by GPUPhysicsWorld
}

void Cloth::GetTriangleVertices(std::vector<glm::vec3>& vertices) const {
    vertices.clear();
    // GPU-based cloth - vertices read directly from GPU buffer
}

void Cloth::GetLineVertices(std::vector<glm::vec3>& vertices) const {
    vertices.clear();
    // GPU-based cloth - vertices read directly from GPU buffer
}

void Cloth::Reset() {
    // GPU-based cloth - reset handled by GPUPhysicsWorld
}

void Cloth::SetGravityEnabled(bool enabled) {
    m_GravityEnabled = enabled;
}

bool Cloth::IsGravityEnabled() const {
    return m_GravityEnabled;
}

void Cloth::SetAllParticlesPinned(bool pinned) {
    // GPU-based cloth - pinning handled by GPUPhysicsWorld::SetParticlesPinned
}

} // namespace cloth
