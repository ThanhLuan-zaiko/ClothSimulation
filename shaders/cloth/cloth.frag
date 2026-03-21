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
uniform sampler2D u_ClothTexture;
uniform int u_UseTexture;

void main() {
    // Wireframe mode
    if (u_Wireframe == 1) {
        out_Color = vec4(u_WireframeColor, 1.0);
        return;
    }

    // Phong lighting
    float ambientStrength = 0.7;
    vec3 ambient = ambientStrength * vec3(0.7, 0.7, 0.75);

    // Diffuse
    vec3 norm = normalize(v_Normal);
    vec3 lightDir = normalize(u_LightPos - v_FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 0.98, 0.95);

    // Specular
    float specularStrength = 0.2;
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * vec3(1.0, 1.0, 1.0) * spec;

    // Sample texture or use color
    vec3 baseColor;
    if (u_UseTexture == 1) {
        baseColor = texture(u_ClothTexture, v_TexCoord).rgb;
    } else {
        baseColor = u_Color;
    }

    // Apply lighting to base color
    vec3 result = (ambient + diffuse + specular) * baseColor;

    // Gamma correction for display
    result = pow(result, vec3(1.0 / 2.2));

    out_Color = vec4(result, 1.0);
}
