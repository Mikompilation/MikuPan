#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec4 uColor;

void main()
{
    FragColor = texture(uTex, vUV) * uColor;
}