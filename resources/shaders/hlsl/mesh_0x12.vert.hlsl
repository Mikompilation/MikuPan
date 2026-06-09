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
    float2 vUV : TEXCOORD0;
    float4 vNormal : TEXCOORD1;
    float4 oViewPosition : TEXCOORD2;
    float4 oWorldPosition : TEXCOORD3;
    float3 oVertexColor : TEXCOORD4;
    float3 oLitVertexColor : TEXCOORD5;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 localPos = float4(input.aPos, 1.0);
    float4 clip = mul(projection, mul(view, mul(model, localPos)));
    output.position = MikuPanFixClipZ(clip);
    output.vUV = input.aUV;

    float3 normalVS = normalize(mul(normalMatrix, input.aNormal));
    float4 normal = float4(normalVS, 1.0);
    output.vNormal = normal;

    float4 viewPosition = mul(modelView, localPos);
    output.oViewPosition = viewPosition;
    output.oWorldPosition = mul(model, localPos);
    output.oVertexColor = input.aColor;
    output.oLitVertexColor = uFlags0.w == 1
        ? CalcPS2LitColor(normal, viewPosition, input.aColor)
        : 0.0.xxx;
    return output;
}
