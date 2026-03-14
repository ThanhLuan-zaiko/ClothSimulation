#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;

out vec4 out_Color;

uniform samplerCube u_EnvironmentMap;
uniform vec3 u_ViewPos;
uniform vec3 u_Color;

void main() {
    // 1. Calculate direction from camera to fragment
    vec3 viewDir = normalize(v_FragPos - u_ViewPos);

    // 2. Reflect direction off the surface normal
    vec3 normal = normalize(v_Normal);
    vec3 reflectDir = reflect(viewDir, normal);

    // 3. Sample environment map (Cubemap)
    vec3 envColor = texture(u_EnvironmentMap, reflectDir).rgb;

    // 4. Fresnel effect for realism
    float fresnel = 1.0 - max(dot(normal, -viewDir), 0.0);
    fresnel = pow(fresnel, 3.0);

    // 5. Final color: Chrome-like reflection
    // 95% reflection, 5% subtle tint from u_Color
    vec3 finalColor = mix(envColor, u_Color, 0.05);

    // Add bright highlight at glancing angles
    finalColor = finalColor + (fresnel * 0.4);

    // Slight brightness boost for chrome look
    out_Color = vec4(finalColor * 1.1, 1.0);
}
