#version 330 core
in vec2 vUV;
in vec4 vNormal;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform int renderNormals;

void main()
{
    vec4 tex = texture(uTexture, vUV);

    if (tex.a == 0.0f)
    {
        discard;
    }

    if (renderNormals == 1)
    {
        FragColor = normalize(vNormal);
    }
    else
    {

        FragColor = vec4(tex.rgb, tex.a);
    }
}