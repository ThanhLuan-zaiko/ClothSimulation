#version 460 core

layout (location = 0) in vec4 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec3 v_Normal;
out vec3 v_FragPos;
out vec2 v_TexCoord;
out float v_IsWireframe;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform int u_Wireframe;

void main() {
    vec3 pos = a_Position.xyz;
    v_FragPos = vec3(u_Model * vec4(pos, 1.0));
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    v_TexCoord = a_TexCoord;
    v_IsWireframe = float(u_Wireframe);

    gl_Position = u_Projection * u_View * u_Model * vec4(pos, 1.0);
}
