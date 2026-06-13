#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float4 aPos : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 vShadowClip : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    float4 localPos = float4(input.aPos.xyz, 1.0);
    float4 worldPos = mul(model, localPos);

    VSOutput output;
    output.position = MikuPanFixClipZ(mul(mvp, localPos));
    output.vShadowClip = mul(uShadowMatrix, worldPos);
    return output;
}
