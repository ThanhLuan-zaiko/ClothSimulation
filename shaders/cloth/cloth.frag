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
uniform float u_Roughness = 0.7;
uniform float u_Anisotropy = 0.5;

uniform sampler2D u_SSAOTexture;
uniform vec2 u_ScreenSize;
uniform int u_UseSSAO;

void main() {
    // ...

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

    vec3 lightDir = normalize(u_LightPos - v_FragPos);
    
    // ============================================================
    // OREN-NAYAR DIFFUSE MODEL
    // ============================================================
    float dotNL = max(dot(norm, lightDir), 0.0);
    float dotNV = max(dot(norm, viewDir), 0.0);
    
    float sigma2 = u_Roughness * u_Roughness;
    float A = 1.0 - 0.5 * (sigma2 / (sigma2 + 0.33));
    float B = 0.45 * (sigma2 / (sigma2 + 0.09));
    
    float theta_i = acos(dotNL);
    float theta_r = acos(dotNV);
    float alpha = max(theta_i, theta_r);
    float beta = min(theta_i, theta_r);
    
    // Azimuthal difference approximation
    vec3 lightProj = normalize(lightDir - norm * dotNL);
    vec3 viewProj = normalize(viewDir - norm * dotNV);
    float cos_phi_diff = max(0.0, dot(lightProj, viewProj));
    
    float oren_nayar = dotNL * (A + B * cos_phi_diff * sin(alpha) * tan(beta));
    vec3 diffuse = oren_nayar * vec3(1.0, 0.98, 0.95);

    // ============================================================
    // ANISOTROPIC SPECULAR (KAJIYA-KAY)
    // ============================================================
    // Calculate tangent from UV derivatives
    vec2 dUVx = dFdx(v_TexCoord);
    vec2 dUVy = dFdy(v_TexCoord);
    vec3 tangent = normalize(dX * dUVy.y - dY * dUVx.y);
    
    float dotLT = dot(tangent, lightDir);
    float dotVT = dot(tangent, viewDir);
    float sinLT = sqrt(max(0.0, 1.0 - dotLT * dotLT));
    float sinVT = sqrt(max(0.0, 1.0 - dotVT * dotVT));
    
    float spec_aniso = pow(max(0.0, sinLT * sinVT - dotLT * dotVT), 32.0);
    vec3 specular = spec_aniso * vec3(1.0) * u_Anisotropy;

    // Ambient
    vec3 ambient = 0.3 * vec3(0.7, 0.7, 0.75);

    vec3 baseColor;
    if (u_UseTexture == 1) {
        baseColor = texture(u_ClothTexture, v_TexCoord).rgb;
    } else {
        baseColor = u_Color;
    }

    vec3 result = (ambient + diffuse + specular) * baseColor;

    // Apply Screen Space Ambient Occlusion (SSAO)
    if (u_UseSSAO == 1) {
        vec2 screenUV = gl_FragCoord.xy / u_ScreenSize;
        float ssao = texture(u_SSAOTexture, screenUV).r;
        result *= ssao;
    }

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
