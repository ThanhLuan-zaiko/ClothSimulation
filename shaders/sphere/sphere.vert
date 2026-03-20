#version 460 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;

out vec3 v_Normal;
out vec3 v_FragPos;
out vec2 v_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    v_FragPos = vec3(u_Model * vec4(a_Position, 1.0));
    v_Normal = normalize(mat3(transpose(inverse(u_Model))) * a_Normal);
    
    // Generate spherical texture coordinates for potential use
    vec3 pos = normalize(a_Position);
    v_TexCoord = vec2(
        0.5 + atan(pos.z, pos.x) / (2.0 * 3.14159265359),
        0.5 - asin(pos.y) / 3.14159265359
    );

    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}
