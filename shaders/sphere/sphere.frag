#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec2 v_TexCoord;

out vec4 out_Color;

// Environment map with LOD support
uniform samplerCube u_EnvironmentMap;
uniform vec3 u_ViewPos;
uniform vec3 u_Color;

// PBR material properties
uniform float u_Metallic;
uniform float u_Roughness;

// HDR tone mapping control
uniform float u_Exposure;
uniform int u_UseHDR;

// LOD bias for performance (blurrier reflection at distance)
uniform float u_LODBias;

void main() {
    vec3 normal = normalize(v_Normal);
    vec3 viewDir = normalize(v_FragPos - u_ViewPos);

    // ========================================
    // 1. REFLECTION WITH MINIMAL LOD BIAS
    // ========================================
    vec3 reflectDir = reflect(viewDir, normal);

    // Use very small LOD bias for sharper reflection (mirror-like)
    float lodLevel = u_LODBias * 0.5; // Reduce LOD bias by half for sharper reflection

    // Sample environment map with LOD
    vec3 envColor;
    if (u_UseHDR == 1) {
        envColor = textureLod(u_EnvironmentMap, reflectDir, lodLevel).rgb;
    } else {
        envColor = texture(u_EnvironmentMap, reflectDir).rgb;
    }

    // ========================================
    // 2. ENHANCED FRESNEL-SCHLICK EFFECT
    // ========================================
    // More accurate Fresnel for dielectric/metallic materials
    float NdotV = max(dot(normal, -viewDir), 0.0);

    // Base reflectivity (F0) - higher for more mirror-like surface
    vec3 F0 = vec3(0.04); // Dielectric base
    F0 = mix(F0, u_Color * 0.95, u_Metallic); // Adjust for metallic (increased from 0.9)

    // Enhanced Schlick approximation with better curve
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 4.0); // Changed from 5.0 to 4.0 for stronger edge reflection

    // ========================================
    // 3. SPECULAR HIGHLIGHT (GGX-like)
    // ========================================
    vec3 lightDir = normalize(vec3(5.0, 10.0, 5.0)); // Match main scene light
    vec3 halfDir = normalize(viewDir + lightDir);
    float NdotH = max(dot(normal, halfDir), 0.0);

    // GGX distribution for more realistic specular
    float specPower = 128.0 * (1.0 - u_Roughness); // Increased from 64.0 for sharper highlights
    float specular = pow(NdotH, specPower);

    // Specular color (brighter for smoother surfaces)
    vec3 specColor = vec3(1.0) * specular * (1.0 - u_Roughness);

    // ========================================
    // 4. HDR TONE MAPPING (High Contrast)
    // ========================================
    vec3 hdrColor = envColor;

    if (u_UseHDR == 1) {
        // Apply exposure (boosted for better contrast)
        hdrColor = envColor * u_Exposure;

        // High contrast ACES tone mapping curve
        const float a = 2.51;
        const float b = 0.03;
        const float c = 2.43;
        const float d = 0.59;
        const float e = 0.14;
        hdrColor = (hdrColor * (a * hdrColor + b)) / (hdrColor * (c * hdrColor + d) + e);
        
        // Boost highlights and shadows for more contrast
        hdrColor = pow(hdrColor, vec3(0.85)); // Gamma < 1.0 increases contrast
        
        // Add slight saturation boost for more vibrant reflection
        float luminance = dot(hdrColor, vec3(0.299, 0.587, 0.114));
        float saturation = 1.3f; // 30% saturation boost
        hdrColor = mix(vec3(luminance), hdrColor, saturation);
    }

    // ========================================
    // 5. FINAL COMPOSITION (Enhanced)
    // ========================================
    // Mix reflection with Fresnel - more reflection, less base color
    vec3 finalColor = mix(hdrColor, u_Color, 0.01 * (1.0 - u_Metallic)); // Reduced base color blend

    // Add specular highlight
    finalColor += specColor * F;

    // Enhanced Fresnel boost at glancing angles (more mirror-like)
    float fresnelBoost = pow(1.0 - NdotV, 2.0) * 0.5; // Changed from 3.0 to 2.0 for stronger boost
    finalColor += fresnelBoost * hdrColor;

    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    out_Color = vec4(finalColor, 1.0);
}
