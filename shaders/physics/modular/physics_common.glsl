#version 460 core
#extension GL_ARB_compute_shader : require
#extension GL_ARB_shader_storage_buffer_object : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require

// GPU-specific settings
#define USE_TEXTURE_GATHER 0
#define MAX_ITERATIONS_RUNTIME 5

layout(local_size_x = 128) in;

layout(std430, binding = 0) buffer PositionBufferSSBO {
    vec4 positions[];
};

layout(std430, binding = 5) buffer PrevPositionBufferSSBO {
    vec4 prevPositions[];
};

layout(std430, binding = 6) buffer VelocityBufferSSBO {
    vec4 velocities[];
};

struct ConstraintDataType {
    ivec4 indices;  // xy = p1,p2 particle indices
    vec2 params;    // x = restLength, y = stiffness
    vec2 padding;
};

layout(std430, binding = 1) buffer ConstraintBufferSSBO {
    ConstraintDataType constraints[];
};

layout(std430, binding = 4) buffer PinnedFlagsSSBO {
    float pinnedFlags[];
};

layout(std430, binding = 9) buffer ConstraintAdjacencySSBO {
    ivec2 particleConstraints[];
};

layout(std430, binding = 7) buffer ParticleColorsSSBO {
    uint particleColors[];
};

layout(std430, binding = 10) buffer ConstraintIndicesSSBO {
    int constraintIndices[];
};

layout(std430, binding = 11) buffer GridBufferSSBO {
    uint gridHead[]; 
};

layout(std430, binding = 12) buffer NextBufferSSBO {
    uint gridNext[]; 
};

const uint GRID_SIZE = 32768u;
const uint GRID_MASK = GRID_SIZE - 1u;
const float CELL_SIZE = 1.05f;

layout(std140, binding = 2) uniform PhysicsParams {
    vec4 gravity_dt;        // xyz = gravity, w = deltaTime
    vec4 params1;           // x = damping, y = numParticles, z = numConstraints, w = restLength
    vec4 terrain;           // xy = terrainSize, z = terrainHeightScale, w = gravityScale
    vec4 forces;            // x = airResistance, y = windStrength, zw = padding
    vec4 windDir_stretch;   // xyz = windDirection, w = stretchResistance
    vec4 limits;            // x = maxVelocity, y = selfCollisionRadius, z = selfCollisionStrength, w = padding
};

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

const float EPSILON = 0.0001f;
const int MAX_ITERATIONS = MAX_ITERATIONS_RUNTIME;

shared vec3 sharedPositions[128];

uint hashPos(vec3 p) {
    ivec3 cell = ivec3(floor(p / CELL_SIZE));
    return (uint(cell.x) * 73856093u ^ uint(cell.y) * 19349663u ^ uint(cell.z) * 83492791u) & GRID_MASK;
}
