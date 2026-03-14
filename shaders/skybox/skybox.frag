#version 460 core

out vec4 out_Color;

in vec3 v_TexCoord;

uniform samplerCube u_Skybox;

void main() {
    out_Color = texture(u_Skybox, v_TexCoord);
}
