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

// ============================================================================
// DEBUG COLOR SYSTEM - 15 Colors for Bug Visualization
// ============================================================================

// Color constants for debug visualization
#define DEBUG_COLOR_NORMAL          vec3(1.0, 1.0, 1.0)    // White - Normal, no issues
#define DEBUG_COLOR_SPHERE_CCD_FAIL vec3(1.0, 0.0, 0.0)    // Red - Sphere CCD failure, penetration
#define DEBUG_COLOR_SPHERE_MARGIN   vec3(1.0, 0.5, 0.0)    // Orange - Sphere margin zone entry
#define DEBUG_COLOR_TERRAIN_CCD_FAIL vec3(1.0, 1.0, 0.0)   // Yellow - Terrain CCD failure
#define DEBUG_COLOR_TERRAIN_MARGIN  vec3(0.0, 1.0, 0.0)    // Lime - Terrain margin zone entry
#define DEBUG_COLOR_SELF_COLLISION  vec3(0.0, 1.0, 1.0)    // Cyan - Self-collision failure
#define DEBUG_COLOR_CONSTRAINT_STRETCH vec3(0.0, 0.0, 1.0) // Blue - Constraint over-stretch
#define DEBUG_COLOR_CONSTRAINT_SHEAR vec3(1.0, 0.0, 1.0)   // Magenta - Shear constraint failure
#define DEBUG_COLOR_BENDING_LIMIT   vec3(0.5, 0.0, 0.5)    // Purple - Bending limit exceeded
#define DEBUG_COLOR_STRAIN_RATE     vec3(1.0, 0.5, 0.5)    // Pink - Strain rate violation
#define DEBUG_COLOR_VELOCITY_EXPLOSION vec3(0.6, 0.3, 0.0) // Brown - Velocity explosion
#define DEBUG_COLOR_PENETRATION     vec3(0.5, 0.0, 0.0)    // Dark Red - Deep penetration
#define DEBUG_COLOR_BUFFER_SYNC     vec3(0.0, 0.5, 0.0)    // Dark Green - Buffer sync issue
#define DEBUG_COLOR_CONSTRAINT_STARVATION vec3(0.0, 0.0, 0.5) // Dark Blue - Constraint starvation
#define DEBUG_COLOR_SLEEPING        vec3(0.5, 0.5, 0.5)    // Gray - Sleeping/jitter zone

// Debug state values (stored in position.w or particleData)
#define DEBUG_STATE_NORMAL              0.0
#define DEBUG_STATE_SPHERE_CCD_FAIL     1.0
#define DEBUG_STATE_SPHERE_MARGIN       2.0
#define DEBUG_STATE_TERRAIN_CCD_FAIL    3.0
#define DEBUG_STATE_TERRAIN_MARGIN      4.0
#define DEBUG_STATE_SELF_COLLISION      5.0
#define DEBUG_STATE_CONSTRAINT_STRETCH  6.0
#define DEBUG_STATE_CONSTRAINT_SHEAR    7.0
#define DEBUG_STATE_BENDING_LIMIT       8.0
#define DEBUG_STATE_STRAIN_RATE         9.0
#define DEBUG_STATE_VELOCITY_EXPLOSION  10.0
#define DEBUG_STATE_PENETRATION         11.0
#define DEBUG_STATE_BUFFER_SYNC         12.0
#define DEBUG_STATE_CONSTRAINT_STARVATION 13.0
#define DEBUG_STATE_SLEEPING            14.0

// Helper function to get color from debug state
vec3 getDebugColor(float state) {
    if (state == DEBUG_STATE_NORMAL) return DEBUG_COLOR_NORMAL;
    if (state == DEBUG_STATE_SPHERE_CCD_FAIL) return DEBUG_COLOR_SPHERE_CCD_FAIL;
    if (state == DEBUG_STATE_SPHERE_MARGIN) return DEBUG_COLOR_SPHERE_MARGIN;
    if (state == DEBUG_STATE_TERRAIN_CCD_FAIL) return DEBUG_COLOR_TERRAIN_CCD_FAIL;
    if (state == DEBUG_STATE_TERRAIN_MARGIN) return DEBUG_COLOR_TERRAIN_MARGIN;
    if (state == DEBUG_STATE_SELF_COLLISION) return DEBUG_COLOR_SELF_COLLISION;
    if (state == DEBUG_STATE_CONSTRAINT_STRETCH) return DEBUG_COLOR_CONSTRAINT_STRETCH;
    if (state == DEBUG_STATE_CONSTRAINT_SHEAR) return DEBUG_COLOR_CONSTRAINT_SHEAR;
    if (state == DEBUG_STATE_BENDING_LIMIT) return DEBUG_COLOR_BENDING_LIMIT;
    if (state == DEBUG_STATE_STRAIN_RATE) return DEBUG_COLOR_STRAIN_RATE;
    if (state == DEBUG_STATE_VELOCITY_EXPLOSION) return DEBUG_COLOR_VELOCITY_EXPLOSION;
    if (state == DEBUG_STATE_PENETRATION) return DEBUG_COLOR_PENETRATION;
    if (state == DEBUG_STATE_BUFFER_SYNC) return DEBUG_COLOR_BUFFER_SYNC;
    if (state == DEBUG_STATE_CONSTRAINT_STARVATION) return DEBUG_COLOR_CONSTRAINT_STARVATION;
    if (state == DEBUG_STATE_SLEEPING) return DEBUG_COLOR_SLEEPING;
    return DEBUG_COLOR_NORMAL;
}

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
// EXTERNAL UNIFORMS (for terrain functions)
// ============================================================================

uniform sampler2D terrainHeightmap;

// ============================================================================
// DEBUG COLOR HELPER FUNCTIONS (after buffer declarations)
// ============================================================================

// Helper function to set particle color in particleData buffer
void setParticleColor(uint idx, vec3 color) {
    // Pack RGB (0-1) into uint: R in bits 0-9, G in bits 10-19, B in bits 20-29
    uint r = uint(clamp(color.r, 0.0, 1.0) * 1023.0);
    uint g = uint(clamp(color.g, 0.0, 1.0) * 1023.0);
    uint b = uint(clamp(color.b, 0.0, 1.0) * 1023.0);
    particleData[idx] = r | (g << 10) | (b << 20);
}

// Helper function to unpack color from particleData
vec3 unpackParticleColor(uint idx) {
    uint colorData = particleData[idx];
    uint r = colorData & 0x3FFu;
    uint g = (colorData >> 10) & 0x3FFu;
    uint b = (colorData >> 20) & 0x3FFu;
    return vec3(r, g, b) / 1023.0;
}

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
// TERRAIN HELPER FUNCTIONS (must be after UBO and macros)
// ============================================================================

float getTerrainHeight(float x, float z) {
    if (terrainSize.x <= 0.0f || terrainSize.y <= 0.0f) return groundLevel;
    float u = (x + terrainSize.x * 0.5f) / terrainSize.x;
    float v = (z + terrainSize.y * 0.5f) / terrainSize.y;
    vec2 uv = clamp(vec2(u, v), 0.0, 1.0);
    float h = textureLod(terrainHeightmap, uv, 0.0).r;
    return max(groundLevel, h);
}

vec3 getTerrainNormal(float x, float z) {
    float step = 0.15;
    float hL = getTerrainHeight(x - step, z);
    float hR = getTerrainHeight(x + step, z);
    float hD = getTerrainHeight(x, z - step);
    float hU = getTerrainHeight(x, z + step);
    return normalize(vec3(hL - hR, 2.0 * step, hD - hU));
}

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
