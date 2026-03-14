#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec3 v_WorldPos;
in vec3 v_ReflectDir;

out vec4 out_Color;

uniform samplerCube u_EnvironmentMap;
uniform vec3 u_Color;

void main() {
    // Sample the environment map using the reflection direction
    // This creates the mirror/chrome effect
    vec3 envColor = texture(u_EnvironmentMap, v_ReflectDir).rgb;
    
    // Add fresnel effect for more realistic mirror (Schlick approximation)
    // Edges reflect more than center
    vec3 viewDir = normalize(v_WorldPos);
    float fresnel = dot(v_Normal, -viewDir);
    fresnel = clamp(1.0 - fresnel, 0.0, 1.0);
    float fresnelPower = pow(fresnel, 2.0);
    
    // Pure chrome/mirror effect - 100% reflection with edge brightening
    vec3 mirrorColor = envColor * (0.7 + 0.3 * fresnelPower);
    
    // Apply sphere color as a very subtle tint (almost pure chrome)
    vec3 finalColor = mirrorColor * u_Color;
    
    // Add slight brightness boost for shiny chrome look
    finalColor = finalColor * 1.2;
    
    out_Color = vec4(finalColor, 1.0);
}
