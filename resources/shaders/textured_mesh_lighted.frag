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
        float NdotL = max(dot(normal, uParDir[i]), 0.0);

        vec3 Lc = uParDiffuse[i].rgb;

        result += baseColor * Lc * NdotL;
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
        float dist = length(L);
        vec3 Ldir = L / dist;

        vec3 Sdir = normalize(uSpotDir[i].xyz);

        float cd = dot(Ldir, Sdir);
        if (cd * cd < uSpotIntens[i].x)
        {
            continue;
        }

        float outerCos = sqrt(uSpotIntens[i].x);
        float innerCos = min(outerCos + 0.15, 0.999);

        float spot = clamp((cd - outerCos) / (innerCos - outerCos), 0.0, 1.0);
        if (spot <= 0.0)
        {
            continue;
        }

        float distAtt = 1.0 / (1.0 + dist);
        float NdotL = max(dot(N.xyz, Ldir), 0.0);

        float colscale =
        (uSpotDiffuse[i].r +
        uSpotDiffuse[i].g +
        uSpotDiffuse[i].b) * uSpotPower[i].x;

        float att = colscale * distAtt * spot;

        result += baseColor * NdotL * att;
    }

    return result;
}

void main()
{
    vec4 tex = texture(uTexture, vUV);

    if (tex.a == 0.0)
    {
        discard;
    }

    // Fog
    float fogFactor = uFog.w * (1.0 / -oViewPosition.z) + uFog.z;
    fogFactor = clamp(fogFactor, uFog.x, uFog.y);
    vec4 color = mix(uFogColor, tex, fogFactor);

    // Lighting
    color.rgb = ApplyPS2Lights(vNormal, oViewPosition, color.rgb);

    if (renderNormals == 1)
    {
        FragColor = vNormal;
    }
    else
    {
        FragColor = color;
    }
}