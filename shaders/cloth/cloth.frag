#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec2 v_TexCoord;
in float v_IsWireframe;

out vec4 out_Color;

uniform vec3 u_Color;
uniform vec3 u_LightPos;
uniform vec3 u_ViewPos;
uniform int u_Wireframe;
uniform vec3 u_WireframeColor;

void main() {
    // Wireframe mode
    if (u_Wireframe == 1) {
        out_Color = vec4(u_WireframeColor, 1.0);
        return;
    }
    
    // Phong lighting
    // Ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * vec3(0.3, 0.3, 0.35);
    
    // Diffuse
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightPos - v_FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 0.95, 0.9);
    
    // Specular
    float specularStrength = 0.3;
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * vec3(1.0, 1.0, 1.0) * spec;
    
    // Add some color variation based on normal
    vec3 colorVariation = u_Color * (0.8 + 0.2 * v_Normal.y);
    
    vec3 result = (ambient + diffuse + specular) * colorVariation;
    out_Color = vec4(result, 1.0);
}
