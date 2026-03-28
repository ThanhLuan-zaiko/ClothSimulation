// GPU-specific settings
#define USE_TEXTURE_GATHER 0
#define MAX_ITERATIONS_RUNTIME 8

// Work group size for compute shaders
layout(local_size_x = 256) in;

// ============================================================================
// SHADER STORAGE BUFFER OBJECTS (SSBO)
// ============================================================================

// Particle positions (w = cloth ID for debugging)
layout(std430, binding = 0) buffer PositionBufferSSBO {
    vec4 positions[];
};

// Previous positions for Verlet integration
layout(std430, binding = 5) buffer PrevPositionBufferSSBO {
    vec4 prevPositions[];
};

// Particle velocities (currently unused in Verlet, but available for debugging)
layout(std430, binding = 6) buffer VelocityBufferSSBO {
    vec4 velocities[];
};

// Constraint data: connects pairs of particles
struct ConstraintDataType {
    ivec4 indices;  // xy = p1, p2 particle indices
    vec2 params;    // x = restLength, y = stiffness
    vec2 padding;
};

layout(std430, binding = 1) buffer ConstraintBufferSSBO {
    ConstraintDataType constraints[];
};

// Pinned flags: particles with flag >= 0.5 are fixed in place
layout(std430, binding = 4) buffer PinnedFlagsSSBO {
    float pinnedFlags[];
};

// Particle constraint adjacency list (for graph coloring)
layout(std430, binding = 9) buffer ConstraintAdjacencySSBO {
    ivec2 particleConstraints[];
};

// Particle colors for graph coloring (parallel constraint solving)
layout(std430, binding = 7) buffer ParticleColorsSSBO {
    uint particleColors[];
};

// Constraint indices for adjacency list
layout(std430, binding = 10) buffer ConstraintIndicesSSBO {
    int constraintIndices[];
};

// Spatial hash grid head (first particle index in each cell)
layout(std430, binding = 11) buffer GridBufferSSBO {
    uint gridHead[];
};

// Spatial hash grid next (linked list pointer)
layout(std430, binding = 12) buffer NextBufferSSBO {
    uint gridNext[];
};

// ============================================================================
// CONSTANTS
// ============================================================================

const uint GRID_SIZE = 32768u;       // Must match GPUPhysicsWorld.cpp
const uint GRID_MASK = GRID_SIZE - 1u;
const float CELL_SIZE = 1.05f;       // Slightly larger than selfCollisionRadius
const float EPSILON = 0.0001f;       // Small value for floating point comparisons
const int MAX_ITERATIONS = MAX_ITERATIONS_RUNTIME;  // Max constraint iterations

// ============================================================================
// UNIFORM BUFFER OBJECTS (UBO)
// ============================================================================

layout(std140, binding = 2) uniform PhysicsParams {
    vec4 gravity_dt;        // xyz = gravity, w = deltaTime
    vec4 params1;           // x = damping, y = numParticles, z = numConstraints, w = restLength
    vec4 terrain;           // xy = terrainSize, z = terrainHeightScale, w = gravityScale
    vec4 forces;            // x = airResistance, y = windStrength, zw = padding
    vec4 windDir_stretch;   // xyz = windDirection, w = stretchResistance
    vec4 limits;            // x = maxVelocity, y = selfCollisionRadius, z = selfCollisionStrength, w = padding
};

// Macros for cleaner code
#define gravity (gravity_dt.xyz)
#define deltaTime (gravity_dt.w)
#define damping (params1.x)
#define numParticles (int(params1.y))
#define numConstraints (int(params1.z))
#define restLength (params1.w)
#define terrainSize (terrain.xy)
#define terrainHeightScale (terrain.z)
#define gravityScale (terrain.w)
#define airResistance (forces.x)
#define windStrength (forces.y)
#define windDirection (windDir_stretch.xyz)
#define stretchResistance (windDir_stretch.w)
#define maxVelocity (limits.x)
#define selfCollisionRadius (limits.y)
#define selfCollisionStrength (limits.z)

// ============================================================================
// SHARED MEMORY
// ============================================================================

shared vec3 sharedPositions[256];

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/// @brief Converts a float to a sortable uint
uint floatToUint(float f) {
    uint u = floatBitsToUint(f);
    return (u & 0x80000000u) != 0 ? ~u : u | 0x80000000u;
}

/// @brief Hash function for spatial hashing
/// @param p World position
/// @return Grid cell index
uint hashPos(vec3 p) {
    ivec3 cell = ivec3(floor(p / CELL_SIZE));
    return (uint(cell.x) * 73856093u ^ uint(cell.y) * 19349663u ^ uint(cell.z) * 83492791u) & GRID_MASK;
}
