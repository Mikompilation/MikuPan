#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float4 vShadowClip : TEXCOORD0;
};

static const float2 kShadowPcfDisk[12] = {
    float2( 0.000,  0.500),
    float2( 0.433,  0.250),
    float2( 0.433, -0.250),
    float2( 0.000, -0.500),
    float2(-0.433, -0.250),
    float2(-0.433,  0.250),
    float2( 0.780,  0.000),
    float2( 0.390,  0.676),
    float2(-0.390,  0.676),
    float2(-0.780,  0.000),
    float2(-0.390, -0.676),
    float2( 0.390, -0.676),
};

float SampleShadowMask(float2 uv)
{
    return uAuxTexture.Sample(uAuxTextureSampler, saturate(uv)).r;
}

float SampleShadowOcclusion(float2 uv)
{
    float radius = max(uShadowSize.z, 0.0);
    if (uShadowSize.w <= 0.5 || radius <= 0.01)
    {
        return SampleShadowMask(uv);
    }

    float2 texel_radius = radius / max(uShadowSize.xy, 1.0.xx);
    float occlusion = SampleShadowMask(uv) * 2.0;
    float total_weight = 2.0;

    [unroll]
    for (int i = 0; i < 12; i++)
    {
        float tap_weight = 1.0 - saturate(length(kShadowPcfDisk[i]) * 0.35);
        occlusion +=
            SampleShadowMask(uv + kShadowPcfDisk[i] * texel_radius) *
            tap_weight;
        total_weight += tap_weight;
    }

    return occlusion / total_weight;
}

float4 main(PSInput input) : SV_Target0
{
    if (input.vShadowClip.w <= 0.0)
    {
        if (uFlags1.z != 0)
        {
            return float4(0.0, 0.0, 0.45, 1.0);
        }
        discard;
    }

    float3 sndc = input.vShadowClip.xyz / input.vShadowClip.w;
    float2 suv = sndc.xy * 0.5 + 0.5;

    if (suv.x < 0.0 || suv.x > 1.0 ||
        suv.y < 0.0 || suv.y > 1.0 ||
        sndc.z < -1.0 || sndc.z > 1.0)
    {
        if (uFlags1.z != 0)
        {
            return float4(0.0, 0.0, 0.45, 1.0);
        }
        discard;
    }

    float occluded = SampleShadowOcclusion(float2(suv.x, 1.0 - suv.y));
    if (uFlags1.z != 0)
    {
        return float4(occluded, 0.35 + 0.65 * (1.0 - occluded), 0.0, 1.0);
    }

    float alpha = occluded * uParams0.y;
    if (alpha <= 0.001)
    {
        discard;
    }

    return float4(0.0, 0.0, 0.0, alpha);
}
