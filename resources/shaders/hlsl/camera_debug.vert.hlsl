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
    float4 clip = mul(uWorldClipView, input.aPos);
    clip.z = clip.z * 2.0 - clip.w;

    VSOutput output;
    output.position = MikuPanFixClipZ(clip);
    output.outColor = uColor;
    return output;
}
