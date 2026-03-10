#version 460 core

in vec3 v_Normal;
in vec3 v_FragPos;
in vec2 v_TexCoord;

out vec4 out_Color;

void main() {
    // Debug: Display bright pink color to verify terrain is rendering
    // If you see pink, the mesh is rendering but texture has issues
    // If you don't see pink, the mesh is not rendering at all
    vec3 debugColor = vec3(1.0, 0.0, 1.0); // Bright magenta
    
    // Add checkerboard pattern to verify texture coordinates
    float checker = mod(floor(v_TexCoord.x * 4.0) + floor(v_TexCoord.y * 4.0), 2.0);
    if (checker > 0.5) {
        debugColor = vec3(0.0, 1.0, 0.0); // Green squares
    }
    
    out_Color = vec4(debugColor, 1.0);
}
