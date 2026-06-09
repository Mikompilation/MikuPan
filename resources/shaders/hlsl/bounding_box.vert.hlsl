#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float4 aPos : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 outColor : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = MikuPanFixClipZ(mul(projection, mul(view, mul(model, input.aPos))));
    output.outColor = uColor;
    return output;
}
