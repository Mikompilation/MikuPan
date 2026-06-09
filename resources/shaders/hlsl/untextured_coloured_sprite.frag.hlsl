#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float4 outColor : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    return input.outColor;
}
