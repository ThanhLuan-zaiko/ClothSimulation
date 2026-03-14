#version 460 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec3 a_Normal;

out vec3 v_Normal;
out vec3 v_FragPos;
out vec3 v_WorldPos;
out vec3 v_ReflectDir;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform vec3 u_ViewPos;

void main() {
    v_FragPos = vec3(u_Model * vec4(a_Position, 1.0));
    v_WorldPos = v_FragPos;
    
    // Transform normal to world space
    v_Normal = normalize(mat3(transpose(inverse(u_Model))) * a_Normal);
    
    // Calculate reflection direction (for environment mapping)
    vec3 viewDir = normalize(v_FragPos - u_ViewPos);
    v_ReflectDir = reflect(-viewDir, v_Normal);
    
    gl_Position = u_Projection * u_View * u_Model * vec4(a_Position, 1.0);
}
