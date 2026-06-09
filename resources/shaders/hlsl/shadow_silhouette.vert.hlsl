#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float4 aPos : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = MikuPanFixClipZ(mul(mvp, float4(input.aPos.xyz, 1.0)));
    return output;
}
