#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float3 aPos : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aUV : TEXCOORD2;
    float3 aColor : TEXCOORD3;
};

struct VSOutput
{
    float4 position : SV_Position;
    float4 outColor : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = MikuPanFixClipZ(mul(mvp, float4(input.aPos, 1.0)));
    output.outColor = float4(abs(normalize(input.aNormal)), 1.0);
    return output;
}
