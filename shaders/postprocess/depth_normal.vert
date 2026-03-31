#version 460 core

layout (location = 0) in vec4 a_Position;
layout (location = 1) in vec3 a_Normal;

out vec3 v_Normal;
out vec3 v_FragPos;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    v_FragPos = vec3(u_View * u_Model * vec4(a_Position.xyz, 1.0));
    v_Normal = mat3(transpose(inverse(u_View * u_Model))) * a_Normal;
    gl_Position = u_Projection * vec4(v_FragPos, 1.0);
}
