#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float2 vDstUV : TEXCOORD1;
    float4 uColor : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target0
{
    float4 col = uTexture.Sample(uTextureSampler, input.vUV) * input.uColor;

    if (uParams1.y > 0.001)
    {
        float strength = clamp(uParams1.y, 0.0, 1.0);
        float3 negative_color = clamp((96.0 / 255.0).xxx - col.rgb, 0.0.xxx, 1.0.xxx);
        col.rgb = lerp(col.rgb, negative_color, strength);
    }

    return col;
}
