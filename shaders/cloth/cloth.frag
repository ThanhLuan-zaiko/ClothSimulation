#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec2 v_TexCoord;
in float v_IsWireframe;
in float v_State;

out vec4 out_Color;

uniform vec3 u_Color;
uniform vec3 u_LightPos;
uniform vec3 u_ViewPos;
uniform int u_Wireframe;
uniform vec3 u_WireframeColor;
uniform sampler2D u_ClothTexture;
uniform int u_UseTexture;
uniform int u_Interactive;
uniform float u_Time;

void main() {
    // Wireframe mode
    if (u_Wireframe == 1) {
        out_Color = vec4(u_WireframeColor, 1.0);
        return;
    }

    // DYNAMIC NORMAL CALCULATION
    vec3 dX = dFdx(v_FragPos);
    vec3 dY = dFdy(v_FragPos);
    vec3 norm = normalize(cross(dX, dY));
    
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    if (dot(norm, viewDir) < 0.0) {
        norm = -norm;
    }

    // Phong lighting
    float ambientStrength = 0.4;
    vec3 ambient = ambientStrength * vec3(0.7, 0.7, 0.75);

    vec3 lightDir = normalize(u_LightPos - v_FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 0.98, 0.95);

    float specularStrength = 0.3;
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
    vec3 specular = specularStrength * vec3(1.0, 1.0, 1.0) * spec;

    vec3 baseColor;
    if (u_UseTexture == 1) {
        baseColor = texture(u_ClothTexture, v_TexCoord).rgb;
    } else {
        baseColor = u_Color;
    }

    vec3 result = (ambient + diffuse + specular) * baseColor;

    // Interactive outline (Green)
    if (u_Interactive == 1) {
        float pulse = 0.5 + 0.5 * sin(u_Time * 6.0);
        float fresnel = 1.0 - max(dot(norm, viewDir), 0.0);
        fresnel = pow(fresnel, 4.0); // Sharper edge
        
        vec3 greenGlow = vec3(0.0, 1.0, 0.0);
        float intensity = 0.4 + 0.6 * pulse;
        result = mix(result, greenGlow, fresnel * intensity);
        
        // Add a slight emissive green tint to everything to show mode is active
        result += vec3(0.0, 0.03 * pulse, 0.0);
    }

    result = pow(result, vec3(1.0 / 2.2));

    out_Color = vec4(result, 1.0);
}
