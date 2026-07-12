#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float4 aPos : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 vViewPos : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    float4 clip = mul(projection, input.aPos);

    VSOutput output;
    output.position = MikuPanFixClipZ(clip);
    output.vViewPos = input.aPos.xyz;
    return output;
}
