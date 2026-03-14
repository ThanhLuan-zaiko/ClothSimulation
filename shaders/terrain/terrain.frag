#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec2 v_TexCoord;

out vec4 out_Color;

uniform sampler2D u_TerrainTexture;

void main() {
    // Sample terrain texture
    vec3 texColor = texture(u_TerrainTexture, v_TexCoord).rgb;
    
    // Simple ambient lighting based on normal (facing up = brighter)
    float ambient = 0.5 + 0.5 * v_Normal.y;
    vec3 finalColor = texColor * ambient;
    
    out_Color = vec4(finalColor, 1.0);
}
