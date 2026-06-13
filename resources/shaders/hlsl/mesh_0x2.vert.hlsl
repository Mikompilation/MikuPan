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
    float4 clip = mul(projection, mul(view, mul(model, input.aPos)));
    output.position = MikuPanFixClipZ(clip);
    output.vUV = input.aUV;

    float3 normalVS = normalize(mul(normalMatrix, input.aNormal.xyz));
    float4 normal = float4(normalVS, 1.0);
    output.vNormal = normal;

    float4 viewPosition = mul(modelView, input.aPos);
    output.oViewPosition = viewPosition;
    output.oWorldPosition = mul(model, input.aPos);
    output.oVertexColor = 0.0.xxx;
    output.oLitVertexColor = uFlags0.w == 1
        ? CalcPS2LitColor(normal, viewPosition, output.oVertexColor)
        : 0.0.xxx;
    return output;
}
