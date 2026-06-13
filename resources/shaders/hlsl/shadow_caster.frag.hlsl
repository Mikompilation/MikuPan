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
    float2 uv = (input.position.xy / max(uShadowSize.xy, 1.0.xx)) * 2.0 - 1.0;
    float r = length(uv);
    float a = 1.0 - smoothstep(0.7, 1.0, r);
    if (a <= 0.0)
    {
        discard;
    }
    return float4(0.0, 0.0, 0.0, a * uParams0.y);
}
