#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

struct VSInput
{
    float3 aPos : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aUV : TEXCOORD2;
    float3 aColor : TEXCOORD3;
    float4 aJoints : TEXCOORD4;
    float4 aWeights : TEXCOORD5;
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

StructuredBuffer<float4> uBonePalette : register(t0, space0);

float3 MikuPanGltfSkinPos(uint bone, float3 p)
{
    float4 c0 = uBonePalette[bone * 4u + 0u];
    float4 c1 = uBonePalette[bone * 4u + 1u];
    float4 c2 = uBonePalette[bone * 4u + 2u];
    float4 c3 = uBonePalette[bone * 4u + 3u];
    return p.x * c0.xyz + p.y * c1.xyz + p.z * c2.xyz + c3.xyz;
}

float3 MikuPanGltfSkinNormal(uint bone, float3 n)
{
    float4 c0 = uBonePalette[bone * 4u + 0u];
    float4 c1 = uBonePalette[bone * 4u + 1u];
    float4 c2 = uBonePalette[bone * 4u + 2u];
    return n.x * c0.xyz + n.y * c1.xyz + n.z * c2.xyz;
}

void MikuPanGltfSkin(VSInput input, out float4 outPos, out float4 outNormal)
{
    float weightSum = dot(input.aWeights, 1.0.xxxx);
    float4 weights = weightSum > 0.000001 ? input.aWeights / weightSum
                                           : float4(1.0, 0.0, 0.0, 0.0);

    float3 skinnedPos = 0.0.xxx;
    float3 skinnedNormal = 0.0.xxx;

    [unroll]
    for (uint i = 0u; i < 4u; i++)
    {
        uint bone = (uint)(input.aJoints[i] + 0.5);
        float weight = weights[i];
        skinnedPos += MikuPanGltfSkinPos(bone, input.aPos) * weight;
        skinnedNormal += MikuPanGltfSkinNormal(bone, input.aNormal) * weight;
    }

    outPos = float4(skinnedPos, 1.0);
    outNormal = float4(normalize(skinnedNormal), 1.0);
}

VSOutput main(VSInput input)
{
    float4 localPos;
    float4 localNormal;
    MikuPanGltfSkin(input, localPos, localNormal);

    VSOutput output;
    float4 clip = mul(projection, mul(view, mul(model, localPos)));
    output.position = MikuPanFixClipZ(clip);
    output.vUV = input.aUV;

    float3 normalVS = normalize(mul(normalMatrix, localNormal.xyz));
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
