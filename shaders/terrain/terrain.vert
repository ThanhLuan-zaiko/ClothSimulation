#version 460 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoord;

out vec3 v_Normal;
out vec3 v_FragPos;
out vec2 v_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    // Transform position to world space
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_FragPos = worldPos.xyz;
    
    // Transform normal to world space
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    v_Normal = normalize(normalMatrix * a_Normal);
    
    // Pass texture coordinates
    v_TexCoord = a_TexCoord;
    
    // Transform to clip space
    gl_Position = u_Projection * u_View * worldPos;
}
