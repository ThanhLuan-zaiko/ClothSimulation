#pragma once

#include "Particle.h"
#include <glm/glm.hpp>

namespace cloth {

enum class ConstraintType {
    Distance,   // Maintains distance between two particles
    Bend,       // Bending constraint for cloth stiffness
    Pin         // Pins a particle to a fixed position
};

struct Constraint {
    Particle* p1;
    Particle* p2;
    float restLength;
    float stiffness;
    ConstraintType type;

    Constraint();
    Constraint(Particle* particle1, Particle* particle2, float stiffness = 1.0f);
    
    void Resolve();
    void UpdateRestLength();
    
    static float CalculateDistance(const Particle* a, const Particle* b);
};

} // namespace cloth
