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
in vec3 oVertexColor;

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
    //vec3 result = uAmbient.rgb * baseColor.rgb;
    vec3 result = vec3(0.0f, 0.0f, 0.0f);

    // Directional (parallel)
    for (int i = 0; i < uParCount.x; i++)
    {
        float NdotL = max(dot(N.xyz, uParDir[i].xyz), 0.0);

        result += baseColor.rgb * uParDiffuse[i].rgb * NdotL;
    }

    // Point
    for (int i = 0; i < uPointCount.x; i++)
    {
        vec3 L = uPointPos[i].xyz - viewPos.xyz;
        float dist = length(L);
        vec3 Ldir = L / dist;

        float NdotL = max(dot(N.xyz, Ldir), 0.0);

        float atten = uPointPower[i].x / (dist);

        result += baseColor.rgb * uPointDiffuse[i].rgb * NdotL * atten;
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

        float atten = 1.0f / dist2;

        float NdotL = max(dot(N.xyz, Ldir), 0.0);

        float w = cone * atten * uSpotPower[i].x;

        result += baseColor.rgb *
        uSpotDiffuse[i].rgb *
        NdotL *
        w;
    }

    vec3 min_color = uAmbient.rgb * baseColor.rgb;
    result.r = clamp(result.r, min_color.r, 1.0f);
    result.g = clamp(result.g, min_color.g, 1.0f);
    result.b = clamp(result.b, min_color.b, 1.0f);

    return result;
}

void main()
{
    vec4 color = texture(uTexture, vUV);

    if (color.a == 0.0)
    {
        discard;
    }

    vec3 albedo = color.rgb * oVertexColor;

    // Fog
    float fogFactor = uFog.w * (1.0 / -oViewPosition.z) + uFog.z;
    fogFactor = clamp(fogFactor, uFog.x, uFog.y);

    // Lighting
    color.rgb = ApplyPS2Lights(vNormal, oViewPosition, albedo.rgb);

    // Fog after lighting
    color = mix(uFogColor, color, fogFactor);

    color.r = clamp(color.r, albedo.r, 1.0f);
    color.g = clamp(color.g, albedo.g, 1.0f);
    color.b = clamp(color.b, albedo.b, 1.0f);

    if (renderNormals == 1)
    {
        FragColor = vec4(oVertexColor, 1.0f);
    }
    else
    {
        FragColor = color;
    }
}