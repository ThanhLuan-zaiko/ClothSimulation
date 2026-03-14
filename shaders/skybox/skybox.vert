#version 460 core

layout (location = 0) in vec3 a_Position;

out vec3 v_TexCoord;

uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    v_TexCoord = a_Position;
    // Remove translation from view matrix to make skybox center at camera
    mat4 view = mat4(mat3(u_View));
    vec4 pos = u_Projection * view * vec4(a_Position, 1.0);
    // Depth is always 1.0 (far plane)
    gl_Position = pos.xyww;
}
