#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float4 vShadowClip : TEXCOORD0;
};

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

    float occluded = uAuxTexture.Sample(uAuxTextureSampler, float2(suv.x, 1.0 - suv.y)).r;
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
