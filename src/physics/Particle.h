#pragma once

#include <glm/glm.hpp>

namespace cloth {

struct Particle {
    glm::vec3 position;      // Current position
    glm::vec3 previousPos;   // Previous position (for Verlet integration)
    glm::vec3 acceleration;  // Current acceleration
    glm::vec3 force;         // Accumulated force
    float mass;
    float damping;
    bool isPinned;           // If true, particle won't move

    Particle();
    Particle(const glm::vec3& pos, float m = 1.0f, bool pinned = false);

    void ApplyForce(const glm::vec3& force);
    void Update(float deltaTime);
    void ResetForce();
    void SetPosition(const glm::vec3& pos);
};

} // namespace cloth
