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

    if (col.a <= 0.0)
    {
        discard;
    }

    return float4(col.a.xxx, col.a);
}
