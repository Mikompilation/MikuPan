#version 330 core

in vec2 vUV;
in vec4 uColor;

out vec4 FragColor;

uniform sampler2D uTexture;

void main()
{
    vec4 tex = texture(uTexture, vUV);
    vec4 col = tex * uColor;

    if (col.a == 0.0f)
    {
        discard;
    }

    FragColor = col;
}