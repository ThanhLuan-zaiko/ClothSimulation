#include "Particle.h"
#include <glm/gtc/constants.hpp>

namespace cloth {

Particle::Particle()
    : position(0.0f),
      previousPos(0.0f),
      acceleration(0.0f),
      force(0.0f),
      mass(1.0f),
      damping(0.98f),
      isPinned(false) {
}

Particle::Particle(const glm::vec3& pos, float m, bool pinned)
    : position(pos),
      previousPos(pos),
      acceleration(0.0f),
      force(0.0f),
      mass(m),
      damping(0.98f),
      isPinned(pinned) {
}

void Particle::ApplyForce(const glm::vec3& f) {
    if (!isPinned) {
        force += f;
    }
}

void Particle::Update(float deltaTime) {
    if (isPinned) return;

    // Verlet integration
    glm::vec3 velocity = (position - previousPos) * damping;
    previousPos = position;
    
    // Update position with acceleration
    position += velocity + acceleration * deltaTime * deltaTime;
    
    // Reset acceleration (will be recalculated from forces)
    acceleration = glm::vec3(0.0f);
}

void Particle::ResetForce() {
    if (!isPinned) {
        force = glm::vec3(0.0f);
    }
}

void Particle::SetPosition(const glm::vec3& pos) {
    if (!isPinned) {
        previousPos = pos;
        position = pos;
    }
}

} // namespace cloth
