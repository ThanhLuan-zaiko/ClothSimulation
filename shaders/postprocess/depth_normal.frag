#version 460 core

layout (location = 0) out vec4 out_Normal; // View-space normal
layout (location = 1) out float out_Depth; // View-space depth

in vec3 v_Normal;
in vec3 v_FragPos;

void main() {
    out_Normal = vec4(normalize(v_Normal), 0.0);
    out_Depth = -v_FragPos.z; // Depth is positive in view space (z is negative)
}
