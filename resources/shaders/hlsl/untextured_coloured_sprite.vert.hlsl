#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float4 aColour : TEXCOORD0;
    float4 aPos : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 outColor : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = MikuPanFixClipZ(input.aPos);
    output.outColor = input.aColour;
    return output;
}
