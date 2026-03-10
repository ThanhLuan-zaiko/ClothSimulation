#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec2 v_TexCoord;

out vec4 out_Color;

uniform sampler2D u_TerrainTexture;

void main() {
    // Debug: Show texture coordinates as color
    // Red = X coordinate, Green = Y coordinate
    // This will show if texture coordinates are correct
    
    vec3 texColor = texture(u_TerrainTexture, v_TexCoord).rgb;
    
    // If texture is black, show coordinate colors instead
    float brightness = dot(texColor, vec3(0.333));
    if (brightness < 0.01) {
        // Texture sampled as black - show coordinates as debug
        out_Color = vec4(v_TexCoord.x, v_TexCoord.y, 0.0, 1.0);
    } else {
        out_Color = vec4(texColor, 1.0);
    }
}
