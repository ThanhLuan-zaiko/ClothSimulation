// GPU-specific settings
#define USE_TEXTURE_GATHER 0
#define MAX_ITERATIONS_RUNTIME 8

// ============================================================================
// SHADER STORAGE BUFFER OBJECTS (SSBO) - OPTIMIZED BINDINGS (max 16)
// ============================================================================
// Binding layout:
//   0  - Positions (vec4)
//   1  - PrevPositions (vec4)
//   2  - Constraints (ConstraintDataType)
//   3  - BendingConstraints (BendingConstraintDataType)
//   4  - DeltaPositions (int) - atomic accumulation
//   5  - PinnedFlags (float)
//   6  - ParticleData (uint) - colors + adjacency combined
//   7  - ConstraintIndices (int)
//   8  - GridHead (uint)
//   9  - GridNext (uint)
//   10 - SortedIndices (uint)
//   11 - SortTemp (uint)
//   12 - CollisionPairs (uvec2)
//   13 - Velocities (vec4) - optional

// Particle positions (w = cloth ID for debugging)
layout(std430, binding = 0) buffer PositionBufferSSBO {
    vec4 positions[];
};

// Previous positions for Verlet integration
layout(std430, binding = 1) buffer PrevPositionBufferSSBO {
    vec4 prevPositions[];
};

// Particle velocities
layout(std430, binding = 13) buffer VelocityBufferSSBO {
    vec4 velocities[];
};

// Constraint data
struct ConstraintDataType {
    ivec4 indices;  // xy = p1, p2 particle indices
    vec2 params;    // x = restLength, y = stiffness
    vec2 padding;
};

layout(std430, binding = 2) buffer ConstraintBufferSSBO {
    ConstraintDataType constraints[];
};

// Dihedral bending constraint data
struct BendingConstraintDataType {
    ivec4 indices;  // p1, p2 (edge), p3, p4 (opposite vertices)
    vec2 params;    // x = restAngle, y = stiffness
    vec2 padding;   // x = anisotropic factor
};

layout(std430, binding = 3) buffer BendingConstraintBufferSSBO {
    BendingConstraintDataType bendConstraints[];
};

// Buffer for atomic accumulation (Fixed-point 10000.0 scale)
layout(std430, binding = 4) buffer DeltaPositionBufferSSBO {
    int deltaPositions[]; // x, y, z interleaved
};

// Pinned flags
layout(std430, binding = 5) buffer PinnedFlagsSSBO {
    float pinnedFlags[];
};

// Combined buffer: colors + adjacency
layout(std430, binding = 6) buffer ParticleDataSSBO {
    uint particleData[];
};

// Constraint indices for adjacency list
layout(std430, binding = 7) buffer ConstraintIndicesSSBO {
    int constraintIndices[];
};

// Spatial hash grid head
layout(std430, binding = 8) buffer GridBufferSSBO {
    uint gridHead[];
};

// Spatial hash grid next
layout(std430, binding = 9) buffer NextBufferSSBO {
    uint gridNext[];
};

// Sorted indices for radix sort
layout(std430, binding = 10) buffer SortedIndicesSSBO {
    uint sortedIndices[];
};

// Temp buffer for radix sort
layout(std430, binding = 11) buffer SortTempSSBO {
    uint sortTemp[];
};

// Collision pairs
layout(std430, binding = 12) buffer CollisionPairsSSBO {
    uvec2 collisionPairs[];
};

// ============================================================================
// CONSTANTS
// ============================================================================

const uint GRID_SIZE = 32768u;
const uint GRID_MASK = GRID_SIZE - 1u;
const float CELL_SIZE = 1.05f;
const float EPSILON = 0.0001f;
const int MAX_ITERATIONS = MAX_ITERATIONS_RUNTIME;
const int MAX_COLORS = 16;

// ============================================================================
// UNIFORM BUFFER OBJECTS (UBO)
// ============================================================================

layout(std140, binding = 0) uniform PhysicsParams {
    vec4 gravity_dt;
    vec4 params1;
    vec4 terrain;
    vec4 forces;
    vec4 windDir_stretch;
    vec4 limits;
    vec4 interaction;
    vec4 time_data;
};

layout(std140, binding = 1) uniform CollisionParams {
    vec3 sphereCenter;
    float sphereRadius;
    float groundLevel;
    float collisionMargin;
    float dampingFactor;
    float frictionFactor;
    int collisionSubsteps;
    float sphereStaticFriction;
    float sphereDynamicFriction;
    float sphereBounce;
    float sphereGripFactor;
    float staticFrictionThreshold;
    float sleepingThreshold;
    float sphereWrapFactor;
    float terrainDamping;
    float ccdSubsteps;
    float conservativeFactor;
    float maxPenetrationCorrection;
};

// Macros
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
#define interactionPos (interaction.xyz)
#define interactionActive (interaction.w > 0.5)
#define u_Time (time_data.x)

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

uint floatToUint(float f) {
    uint u = floatBitsToUint(f);
    uint mask = (u & 0x80000000u) != 0u ? 0xFFFFFFFFu : 0x80000000u;
    return u ^ mask;
}

uint hashPos(vec3 p) {
    ivec3 cell = ivec3(floor(p / CELL_SIZE));
    return (uint(cell.x) * 73856093u ^ uint(cell.y) * 19349663u ^ uint(cell.z) * 83492791u) & GRID_MASK;
}

// Get particle color
uint getParticleColor(uint idx) {
    return particleData[idx];
}
