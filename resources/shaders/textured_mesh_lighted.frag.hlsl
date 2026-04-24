// Textured mesh fragment shader with PS2-style lighting model and fog.

[[vk::combinedImageSampler]]
Texture2D    uTexture : register(t0, space2);
[[vk::combinedImageSampler]]
SamplerState uSampler : register(s0, space2);

cbuffer LightBlock : register(b0, space3)
{
    float4 uAmbient;

    int4   uParCount;
    float4 uParDir[3];
    float4 uParDiffuse[3];

    int4   uPointCount;
    float4 uPointPos[3];
    float4 uPointDiffuse[3];
    float4 uPointPower[3];

    int4   uSpotCount;
    float4 uSpotPos[3];
    float4 uSpotDir[3];
    float4 uSpotDiffuse[3];
    float4 uSpotPower[3];
    float4 uSpotIntens[3];
};

cbuffer FragMiscUniforms : register(b1, space3)
{
    int   renderNormals;
    float uColorScale;
    float2 _pad;
    float4 uFog;       // x=min, y=max, z=base, w=scale
    float4 uFogColor;
};

struct PSInput
{
    float4 Position      : SV_Position;
    float2 vUV           : TEXCOORD0;
    float4 vNormal       : TEXCOORD1;
    float4 oViewPosition : TEXCOORD2;
};

float3 ApplyPS2Lights(float4 normal, float4 viewPos, float3 baseColor)
{
    float3 N      = normalize(normal.xyz);
    float3 result = uAmbient.rgb * baseColor;

    // Directional (parallel) lights
    for (int i = 0; i < uParCount.x; i++)
    {
        float NdotL    = max(dot(N, uParDir[i].xyz), 0.0);
        float colscale = (uParDiffuse[i].r + uParDiffuse[i].g + uParDiffuse[i].b) / 3.0;
        result += baseColor * colscale * NdotL;
    }

    // Point lights
    for (int i = 0; i < uPointCount.x; i++)
    {
        float3 L      = uPointPos[i].xyz - viewPos.xyz;
        float  dist   = length(L);
        float3 Ldir   = L / dist;
        float  NdotL  = max(dot(N, Ldir), 0.0);
        float  colscale = (uPointDiffuse[i].r + uPointDiffuse[i].g + uPointDiffuse[i].b)
                          * uPointPower[i].x;
        float  distAtt = 1.0 / (1.0 + dist);
        result += baseColor * NdotL * colscale * distAtt;
    }

    // Spot lights
    for (int i = 0; i < uSpotCount.x; i++)
    {
        float3 L     = uSpotPos[i].xyz - viewPos.xyz;
        float  dist2 = length(L);
        float3 Ldir  = normalize(L);
        float3 Z     = normalize(uSpotDir[i].xyz);
        float  cd    = max(dot(Ldir, Z), 0.0);
        float  cone  = cd * cd;

        if (cone < uSpotIntens[i].x) continue;

        float atten = 1.0 / dist2;
        float NdotL = max(dot(N, Ldir), 0.0);
        float w     = cone * atten * uSpotPower[i].x;
        result += baseColor * uSpotDiffuse[i].rgb * NdotL * w;
    }

    return result;
}

float4 main(PSInput input) : SV_Target
{
    float4 color = uTexture.Sample(uSampler, input.vUV);

    if (color.a == 0.0) discard;

    // Fog
    float fogFactor = uFog.w * (1.0 / -input.oViewPosition.z) + uFog.z;
    fogFactor = clamp(fogFactor, uFog.x, uFog.y);

    // Lighting
    //color.rgb = ApplyPS2Lights(input.vNormal, input.oViewPosition, color.rgb);
    //color     = lerp(uFogColor, color, fogFactor);

    return (renderNormals == 1) ? input.vNormal : color;
}
