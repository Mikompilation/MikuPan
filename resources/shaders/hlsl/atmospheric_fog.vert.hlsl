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
    float3 vWorldPos : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    float4 world_pos = float4(input.aPos.xyz, 1.0);
    float4 view_pos = mul(view, world_pos);
    float4 clip = mul(projection, view_pos);

    VSOutput output;
    output.position = MikuPanFixClipZ(clip);
    output.vViewPos = view_pos.xyz;
    output.vWorldPos = world_pos.xyz;
    return output;
}
