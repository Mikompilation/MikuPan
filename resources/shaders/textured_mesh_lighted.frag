#version 330 core
in vec2 vUV;
in vec4 vNormal;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform int renderNormals;
uniform vec3 lightColor;
uniform float ambientStrength;

void main()
{
    vec4 tex = texture(uTexture, vUV);

    if (tex.a == 0.0f)
    {
        discard;
    }

    vec3 ambient = ambientStrength * lightColor;

    if (renderNormals == 1)
    {
        //FragColor = transpose(inverse(model)) * aNormal;
        FragColor = vNormal;
    }
    else
    {
        FragColor = tex * vec4(ambient, 1.0f);
    }
}