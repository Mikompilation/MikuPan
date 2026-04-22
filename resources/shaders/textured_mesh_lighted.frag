#version 330 core

layout(std140) uniform LightBlock
{
    vec4 uAmbient;

    ivec4 uParCount;
    vec4 uParDir[3];
    vec4 uParDiffuse[3];

    ivec4 uPointCount;
    vec4 uPointPos[3];
    vec4 uPointDiffuse[3];
    vec4 uPointPower[3];

    ivec4 uSpotCount;
    vec4 uSpotPos[3];
    vec4 uSpotDir[3];
    vec4 uSpotDiffuse[3];
    vec4 uSpotPower[3];
    vec4 uSpotIntens[3];
};

/// Input variables from vertex shader
in vec2 vUV;
in vec4 vNormal;
in vec4 oViewPosition;

out vec4 FragColor;

/// Texture Uniforms
uniform sampler2D uTexture;
uniform int renderNormals;

/// Fog uniforms (kept as regular uniforms)
uniform vec4 uFog;      // x=min, y=max, z=base, w=scale
uniform vec4 uFogColor;

uniform float uColorScale;

vec3 ApplyPS2Lights(vec4 normal, vec4 viewPos, vec3 baseColor)
{
    vec4 N = vec4(normalize(normal.xyz), normal.w);
    vec3 result = uAmbient.rgb * baseColor;

    // Directional (parallel)
    for (int i = 0; i < uParCount.x; i++)
    {
        float NdotL = max(dot(N.xyz, uParDir[i].xyz), 0.0);

        float colscale = (uParDiffuse[i].r +
        uParDiffuse[i].g +
        uParDiffuse[i].b) / 3.0f;

        result += baseColor * colscale * NdotL;
    }

    // Point
    for (int i = 0; i < uPointCount.x; i++)
    {
        vec3 L = uPointPos[i].xyz - viewPos.xyz;
        float dist = length(L);
        vec3 Ldir = L / dist;

        float NdotL = max(dot(N.xyz, Ldir), 0.0);

        float colscale =
        (uPointDiffuse[i].r +
        uPointDiffuse[i].g +
        uPointDiffuse[i].b) * uPointPower[i].x;

        float distAtt = 1.0 / (1.0 + dist);
        float att = colscale * distAtt;

        result += baseColor * NdotL * att;
    }

    // Spot
    for (int i = 0; i < uSpotCount.x; i++)
    {
        vec3 L = uSpotPos[i].xyz - viewPos.xyz;

        float dist2 = length(L);
        vec3 Ldir = normalize(L);

        vec3 Z = normalize(uSpotDir[i].xyz);

        float cd = max(dot(Ldir, Z), 0.0);
        float cone = cd * cd;

        if (cone < uSpotIntens[i].x)
        continue;

        float atten = 1.0 / dist2;

        float NdotL = max(dot(N.xyz, Ldir), 0.0);

        float w = cone * atten * uSpotPower[i].x;

        result += baseColor *
        uSpotDiffuse[i].rgb *
        NdotL *
        w;
    }

    return result;
}

void main()
{
    vec4 color = texture(uTexture, vUV);

    if (color.a == 0.0)
    {
        discard;
    }

    // Fog
    float fogFactor = uFog.w * (1.0 / -oViewPosition.z) + uFog.z;
    fogFactor = clamp(fogFactor, uFog.x, uFog.y);


    // Lighting
    color.rgb = ApplyPS2Lights(vNormal, oViewPosition, color.rgb);

    color = mix(uFogColor, color, fogFactor);

    if (renderNormals == 1)
    {
        FragColor = vNormal;
    }
    else
    {
        FragColor = color;
    }
}