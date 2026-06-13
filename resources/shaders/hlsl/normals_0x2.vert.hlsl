#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float4 aPos : TEXCOORD0;
    float4 aNormal : TEXCOORD1;
    float2 aUV : TEXCOORD2;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 outColor : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = MikuPanFixClipZ(mul(mvp, input.aPos));
    output.outColor = float4(abs(normalize(input.aNormal.xyz)), 1.0);
    return output;
}
