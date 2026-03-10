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
    if (p1->isPinned && p2->isPinned) return;

    glm::vec3 delta = p2->position - p1->position;
    float currentLength = glm::length(delta);
    
    if (currentLength < glm::epsilon<float>()) return;

    glm::vec3 direction = delta / currentLength;
    float displacement = (currentLength - restLength) * stiffness;

    // Apply corrections based on pinning state
    if (!p1->isPinned && !p2->isPinned) {
        // Both particles can move - split the correction
        p1->position += direction * displacement * 0.5f;
        p2->position -= direction * displacement * 0.5f;
    } else if (!p1->isPinned) {
        // Only p1 can move
        p1->position += direction * displacement;
    } else if (!p2->isPinned) {
        // Only p2 can move
        p2->position -= direction * displacement;
    }
}

} // namespace cloth
