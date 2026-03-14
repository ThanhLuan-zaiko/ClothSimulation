#include "Constraint.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace cloth {

Constraint::Constraint()
    : p1(nullptr), p2(nullptr), restLength(0.0f), stiffness(1.0f), type(ConstraintType::Distance) {
}

Constraint::Constraint(Particle* particle1, Particle* particle2, float stiff)
    : p1(particle1), p2(particle2), stiffness(stiff), type(ConstraintType::Distance) {
    UpdateRestLength();
}

float Constraint::CalculateDistance(const Particle* a, const Particle* b) {
    return glm::length(a->position - b->position);
}

void Constraint::UpdateRestLength() {
    if (p1 && p2) {
        restLength = CalculateDistance(p1, p2);
    }
}

void Constraint::Resolve() {
    if (!p1 || !p2) return;
    
    // Skip if both are pinned - optimization
    if (p1->isPinned && p2->isPinned) return;

    glm::vec3 delta = p2->position - p1->position;
    
    // Optimization: Calculate length only once
    float currentLength = glm::length(delta);
    
    // Stability: Use a slightly larger epsilon
    if (currentLength < 0.0001f) return;

    // Displacement ratio
    float diff = (currentLength - restLength) / currentLength;
    glm::vec3 correction = delta * diff * stiffness;

    // Apply corrections based on pinning state
    if (!p1->isPinned && !p2->isPinned) {
        p1->position += correction * 0.5f;
        p2->position -= correction * 0.5f;
    } else if (!p1->isPinned) {
        p1->position += correction;
    } else if (!p2->isPinned) {
        p2->position -= correction;
    }
}

} // namespace cloth
