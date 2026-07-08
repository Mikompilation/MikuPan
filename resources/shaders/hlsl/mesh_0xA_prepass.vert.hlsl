#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float2 aSource : TEXCOORD0;
    float2 aUV : TEXCOORD1;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vNormal : TEXCOORD1;
    float4 oViewPosition : TEXCOORD2;
    float4 oWorldPosition : TEXCOORD3;
    float3 oVertexColor : TEXCOORD4;
    float3 oLitVertexColor : TEXCOORD5;
};

StructuredBuffer<float4> uPrepassBuffer : register(t0, space0);

VSOutput main(VSInput input)
{
    uint posIndex = (uint)(input.aSource.x + 0.5);
    uint nrmIndex = (uint)(input.aSource.y + 0.5);
    float4 aPos = uPrepassBuffer[posIndex * 2u + 0u];
    float4 aNormal = uPrepassBuffer[nrmIndex * 2u + 1u];

    VSOutput output;
    float4 clip = mul(projection, mul(view, aPos));
    output.position = MikuPanFixClipZ(clip);
    output.vUV = input.aUV;

    float3 normalVS = normalize(mul((float3x3)view, aNormal.xyz));
    float4 normal = float4(normalVS, 1.0);
    output.vNormal = normal;

    float4 viewPosition = mul(view, aPos);
    output.oViewPosition = viewPosition;
    output.oWorldPosition = aPos;
    output.oVertexColor = 0.0.xxx;
    output.oLitVertexColor = uFlags0.w == 1
        ? CalcPS2LitColor(normal, viewPosition, output.oVertexColor)
        : 0.0.xxx;
    return output;
}
