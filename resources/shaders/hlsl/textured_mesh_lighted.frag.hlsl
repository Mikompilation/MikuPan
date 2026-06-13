#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vNormal : TEXCOORD1;
    float4 oViewPosition : TEXCOORD2;
    float4 oWorldPosition : TEXCOORD3;
    float3 oVertexColor : TEXCOORD4;
    float3 oLitVertexColor : TEXCOORD5;
};

float4 main(PSInput input) : SV_Target0
{
    float4 color = uTexture.Sample(uTextureSampler, input.vUV);

    if (uFlags0.x == 1)
    {
        return input.vNormal;
    }

    if (color.a == 0.0)
    {
        discard;
    }

    if (uFlags0.z == 1)
    {
        return float4(input.oVertexColor, 1.0);
    }

    if (uFlags0.y == 1)
    {
        return color;
    }

    color.a *= clamp(uMaterialAlpha.x, 0.0, 1.0);

    float3 ps2LightColor = uFlags0.w == 1
        ? input.oLitVertexColor
        : CalcPS2LitColor(input.vNormal, input.oViewPosition, input.oVertexColor);
    color.rgb = ApplyGsModulate(color.rgb, ps2LightColor);

    if (uFlags1.x == 0)
    {
        float fogFactor = uFog.w * (1.0 / -input.oViewPosition.z) + uFog.z;
        fogFactor = clamp(fogFactor, uFog.x, uFog.y);
        color.rgb = lerp(uFogColor.rgb, color.rgb, fogFactor);
    }

    if (uFlags1.y == 1)
    {
        float4 shadowClip = mul(uShadowMatrix, input.oWorldPosition);
        if (shadowClip.w > 0.0)
        {
            float3 sndc = shadowClip.xyz / shadowClip.w;
            float2 suv = sndc.xy * 0.5 + 0.5;
            if (suv.x >= 0.0 && suv.x <= 1.0 &&
                suv.y >= 0.0 && suv.y <= 1.0 &&
                sndc.z >= -1.0 && sndc.z <= 1.0)
            {
                float occluded = uAuxTexture.Sample(uAuxTextureSampler, suv).r;
                color.rgb *= 1.0 - occluded * uParams0.y;
            }
        }
    }

    return color;
}
