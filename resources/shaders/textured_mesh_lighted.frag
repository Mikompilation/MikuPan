#version 330 core

in vec2 vUV;
in vec4 vNormal;
in vec4 oViewPosition;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform int renderNormals;

/// Uniforms for fog
uniform vec4 uFog;             // x=min, y=max, z=base, w=scale
uniform vec4 uFogColor;

#define MAXL 16

/// Uniforms for ambient light
uniform vec4 uAmbient;

// parallel (directional)
uniform int  uParCount;
uniform vec4 uParDir[MAXL];
uniform vec4 uParDiffuse[MAXL];

// point
uniform int  uPointCount;
uniform vec4 uPointPos[MAXL];
uniform vec4 uPointDiffuse[MAXL];
uniform float uPointPower[MAXL];

// spot
uniform int  uSpotCount;
uniform vec4 uSpotPos[MAXL];
uniform vec4 uSpotDir[MAXL];
uniform vec4 uSpotDiffuse[MAXL];
uniform float uSpotPower[MAXL];
uniform float uSpotIntens[MAXL];

vec3 ApplyPS2Lights(vec4 normal, vec4 viewPos, vec3 baseColor)
{
    vec4 N = normalize(normal);
    vec3 result = uAmbient.rgb * baseColor;

    // Directional (parallel)
    for (int i = 0; i < uParCount; i++)
    {
        float NdotL = max(dot(N.xyz, normalize(uParDir[i].xyz)), 0.0);
        result += baseColor * uParDiffuse[i].rgb * NdotL;
    }

    // Point
    for (int i = 0; i < uPointCount; i++)
    {
        vec3 L = uPointPos[i].xyz - viewPos.xyz;
        float dist = length(L.xyz);
        vec3 Ldir = L.xyz / (dist);

        float NdotL = max(dot(N.xyz, Ldir), 0.0);

        float colscale = (uSpotDiffuse[i].r + uSpotDiffuse[i].g + uSpotDiffuse[i].b) * uSpotPower[i];
        float att = colscale / dist; // distance attenuation

        result += baseColor * uPointDiffuse[i].rgb * NdotL * att;
    }

    // Spot
    for (int i = 0; i < uSpotCount; i++)
    {
        vec3 L = uSpotPos[i].xyz - viewPos.xyz; // from light to fragment
        float dist = length(L);
        vec3 Ldir = L / (dist);

        float plane = dot(normalize(uSpotDir[i].xyz), L);
        if (plane <= 0.0) continue;

        float NdotL = max(dot(N.xyz, Ldir), 0.0);

        float cosAngle = max(dot(Ldir, normalize(uSpotDir[i].xyz)), 0.0);
        float cone = (cosAngle) * uSpotIntens[i]; // intensity exponent controls cone sharpness

        float cd = dot(normalize(L), normalize(uSpotDir[i].xyz));
        if (cd * cd < uSpotIntens[i])
        {
            continue;
        }

        float colscale = (uSpotDiffuse[i].r + uSpotDiffuse[i].g + uSpotDiffuse[i].b) * uSpotPower[i];

        float att = colscale / (dist + 1.0f); // distance attenuation

        result += baseColor * NdotL * att;
    }

    return result;
}

void main()
{
    // Sample texture
    vec4 tex = texture(uTexture, vUV);

    // Early discard for fully transparent pixels
    if(tex.a == 0.0)
    {
        discard;
    }

    vec4 color = tex;

    // Apply fog
    float fogFactor = uFog.w * (1.0 / -oViewPosition.z) + uFog.z;
    fogFactor = clamp(fogFactor, uFog.x, uFog.y);
    color = mix(uFogColor, color, fogFactor);

    color.rgb = ApplyPS2Lights(vNormal, oViewPosition, color.rgb);
    color.a = color.a;

    // Optionally render normals
    if(renderNormals == 1)
    {
        FragColor = vNormal;
    }
    else
    {
        FragColor = color;
    }
}