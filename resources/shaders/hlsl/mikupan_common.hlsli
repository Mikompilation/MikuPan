#ifndef MIKUPAN_HLSL_COMMON
#define MIKUPAN_HLSL_COMMON

#ifdef MIKUPAN_VERTEX_STAGE
#define MIKUPAN_UNIFORM_SPACE space1
#else
#define MIKUPAN_UNIFORM_SPACE space3
#endif

cbuffer MikuPanUniforms : register(b0, MIKUPAN_UNIFORM_SPACE)
{
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 projection;
    column_major float4x4 mvp;
    column_major float4x4 modelView;
    column_major float4x4 viewProj;
    column_major float4x4 uShadowMatrix;
    column_major float4x4 uWorldClipView;

    column_major float3x3 normalMatrix;
    column_major float3x3 viewNormalMatrix;

    float4 uColor;
    float4 uFog;
    float4 uFogColor;
    float4 uShadowSize;
    float4 uTextureSize;
    float4 uOutputSize;
    float4 uPhotoNegativeContentRect;
    float4 uPhotoNegativeRect;
    float4 uFramebufferUvOffset;
    float4 uFramebufferUvScale;
    float4 uFramebufferContentUvMax;
    float4 uRenderSize;

    float4 uParams0; // x=normal length, y=shadow strength, z=brightness, w=gamma
    float4 uCrt0;    // x=strength, y=curvature, z=overscan, w=scanline strength
    float4 uCrt1;    // x=scanline scale, y=thickness, z=mask strength, w=mask scale
    float4 uCrt2;    // x=vignette strength, y=size, z=chroma offset, w=blend strength
    float4 uCrt3;    // x=blend radius, y=noise, z=flicker, w=glow
    float4 uParams1; // x=time, y=photo negative strength, z=final output curve
    float4 uPs2Feedback; // x=strength, y=burn, z=saturation, w=ghost
    float4 uScreenNegative; // rgb=Himuro negative colour, a=strength

    int4 uFlags0; // x=renderNormals, y=disableLighting, z=staticLighting, w=meshLightingMode
    int4 uFlags1; // x=mirrorSurfacePass, y=shadowEnabled, z=shadowDebugView, w=crtEnabled
    int4 uFlags2; // x=blackWhiteMode, y=photoNegativeEnabled, z=photoNegativeSourceEnabled, w=screenCopyMode
    int4 uPadFlags; // x=ps2FeedbackEnabled, y=ps2FeedbackPreviousEnabled
};

cbuffer LightBlock : register(b1, MIKUPAN_UNIFORM_SPACE)
{
    float4 uAmbient;

    int4 uParCount;
    float4 uParDir[3];
    float4 uParDiffuse[3];
    float4 uParSpecular[3];
    float4 uParHalfway[3];

    int4 uPointCount;
    float4 uPointPos[3];
    float4 uPointDiffuse[3];
    float4 uPointSpecular[3];
    float4 uPointPower[3];

    int4 uSpotCount;
    float4 uSpotPos[3];
    float4 uSpotDir[3];
    float4 uSpotDiffuse[3];
    float4 uSpotSpecular[3];
    float4 uSpotPower[3];
    float4 uSpotIntens[3];
    float4 uMaterialAlpha;
};

cbuffer MaterialBlock : register(b2, MIKUPAN_UNIFORM_SPACE)
{
    float4 uMatAmbient;
    float4 uMatDiffuse;
    float4 uMatSpecular;
    float4 uMatEmission;
};

#ifndef MIKUPAN_VERTEX_STAGE
Texture2D uTexture : register(t0, space2);
SamplerState uTextureSampler : register(s0, space2);
Texture2D uAuxTexture : register(t1, space2);
SamplerState uAuxTextureSampler : register(s1, space2);
#endif

static const float kGsModulateScale = 255.0 / 128.0;

float4 MikuPanFixClipZ(float4 clip)
{
    clip.z = clip.z * 0.5 + clip.w * 0.5;
    return clip;
}

float3 ApplyGsModulate(float3 textureColor, float3 ps2Color255)
{
    return clamp(clamp(ps2Color255, 0.0.xxx, 1.0.xxx) *
                 textureColor * kGsModulateScale, 0.0.xxx, 1.0.xxx);
}

float3 CalcPS2LitColor(float4 normal, float4 viewPos, float3 vertexColor)
{
    float3 N = normalize(normal.xyz);
    const float parallel_shininess = 4.0;
    float3 vc = vertexColor + uAmbient.rgb;

    for (int i = 0; i < uParCount.x; i++)
    {
        float NdotL = max(dot(N, uParDir[i].xyz), 0.0);
        float NdotH = max(dot(N, uParHalfway[i].xyz), 0.0);
        vc += uParDiffuse[i].rgb * NdotL;
        vc += uParSpecular[i].rgb * pow(NdotH, parallel_shininess);
    }

    for (int j = 0; j < uPointCount.x; j++)
    {
        float3 L = uPointPos[j].xyz - viewPos.xyz;
        float dist2 = dot(L, L);
        if (dist2 <= 0.0)
        {
            continue;
        }

        float inv_dist = rsqrt(dist2);
        float3 Ldir = L * inv_dist;
        float NdotL = max(dot(N, Ldir), 0.0);
        float intensity = min(NdotL * uPointPower[j].x * inv_dist, 1.0);

        vc += uPointDiffuse[j].rgb * intensity;

        float spec = intensity * intensity;
        spec *= spec;
        spec *= spec;
        vc += uPointSpecular[j].rgb * spec;
    }

    for (int k = 0; k < uSpotCount.x; k++)
    {
        float3 L = uSpotPos[k].xyz - viewPos.xyz;
        float dist2 = dot(L, L);
        if (dist2 <= 0.0)
        {
            continue;
        }

        float inv_dist = rsqrt(dist2);
        float3 Ldir = L * inv_dist;
        float3 Z = normalize(uSpotDir[k].xyz);
        float cd = max(dot(Ldir, Z), 0.0);
        float cos2 = cd * cd;
        float gate = max(cos2 - uSpotIntens[k].x, 0.0) * uSpotIntens[k].y;
        gate = clamp(gate, 0.0, 1.0);
        if (cos2 <= uSpotIntens[k].x)
        {
            continue;
        }

        float NdotL = max(dot(N, Ldir), 0.0);
        float intensity = min(NdotL * uSpotPower[k].x * inv_dist, 1.0);

        vc += uSpotDiffuse[k].rgb * intensity * gate;

        float spec = intensity * intensity;
        spec *= spec;
        spec *= spec;
        vc += uSpotSpecular[k].rgb * spec * gate;
    }

    return vc;
}

float3 ToBlackWhite(float3 color)
{
    float gray = (color.r + color.g + color.b) / 3.0;
    return gray.xxx;
}

float3 ApplyBlackWhiteLightOnly(float3 color)
{
    return uFlags2.x != 0 ? ToBlackWhite(color) : color;
}

#endif
