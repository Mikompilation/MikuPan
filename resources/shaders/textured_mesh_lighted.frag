#version 330 core
in vec2 vUV;
in vec4 vNormal;
in float outViewZCoord;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform int renderNormals;
uniform vec3 lightColor;
uniform float ambientStrength;

uniform vec4 uFog;
uniform vec4 uFogColor;

void main()
{
    vec4 tex = texture(uTexture, vUV);

    if (tex.a == 0.0f)
    {
        discard;
    }

    vec3 ambient = ambientStrength * lightColor;

    if (ambient.x == 0.0f && ambient.y == 0.0f && ambient.z == 0.0f)
    {
        ambient = vec3(1.0f, 1.0f, 1.0f);
    }

    vec4 color = tex * vec4(ambient, 1.0f);

    float fog = uFog.w * (1.0 / -outViewZCoord) + uFog.z;
    float vFog = clamp(fog, uFog.x, uFog.y);
    color = mix(uFogColor, color, vFog);

    if (renderNormals == 1)
    {
        //FragColor = transpose(inverse(model)) * aNormal;
        FragColor = vNormal;
    }
    else
    {
        FragColor = color;
    }
}