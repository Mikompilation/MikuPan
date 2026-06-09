#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float4 aUV : TEXCOORD0;
    float4 inColor : TEXCOORD1;
    float4 aPos : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 vUVData : TEXCOORD0;
    float4 uColor : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = MikuPanFixClipZ(input.aPos);
    output.vUVData = input.aUV;
    output.uColor = input.inColor;
    return output;
}
