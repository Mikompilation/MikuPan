#define MIKUPAN_VERTEX_STAGE
#include "mikupan_common.hlsli"

// GPU 2-bone linear-blend skin. Reproduces calc_skinned_data() from libsg.c:
// each vertex carries two bind positions / normals (one per bone) plus a blend
// weight; the per-frame bone-matrix palette arrives in a vertex storage buffer.
// The blended world-space position/normal are then fed through the exact tail
// of mesh_0xA.vert so the result matches the old CPU-skinned path.
struct VSInput
{
    float4 aBonePos0  : TEXCOORD0; // bone-0 bind pos (xyz) + blend weight (w)
    float4 aBonePos1  : TEXCOORD1; // bone-1 bind pos (xyz) + packed indices (w)
    float4 aBoneNorm0 : TEXCOORD2; // bone-0 bind normal (xyz) + normal weight (w)
    float4 aBoneNorm1 : TEXCOORD3; // bone-1 bind normal (xyz)
    float2 aUV        : TEXCOORD4;
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

// Bone-matrix palette: each bone occupies four consecutive float4 rows, laid
// out exactly like sceVu0FMATRIX (row-major lwmtx). Row 3's w component holds
// the PS2 normal-scale factor (m0[3][3]).
StructuredBuffer<float4> uBonePalette : register(t0, space0);

float3 MikuPanSkinPos(uint bone, float3 p)
{
    float4 r0 = uBonePalette[bone * 4u + 0u];
    float4 r1 = uBonePalette[bone * 4u + 1u];
    float4 r2 = uBonePalette[bone * 4u + 2u];
    float4 r3 = uBonePalette[bone * 4u + 3u];
    return p.x * r0.xyz + p.y * r1.xyz + p.z * r2.xyz + r3.xyz;
}

float3 MikuPanSkinNrm(uint bone, float3 n)
{
    float4 r0 = uBonePalette[bone * 4u + 0u];
    float4 r1 = uBonePalette[bone * 4u + 1u];
    float4 r2 = uBonePalette[bone * 4u + 2u];
    return n.x * r0.xyz + n.y * r1.xyz + n.z * r2.xyz;
}

void MikuPanSkin(VSInput input, out float4 aPos, out float4 aNormal)
{
    float w0 = input.aBonePos0.w;
    float w1 = 1.0 - w0;

    // Bone index pair encoded as a normal float (b0 + b1*256) on the CPU.
    uint packed = (uint)(input.aBonePos1.w + 0.5);
    uint b0 = packed & 0xFFu;
    uint b1 = (packed >> 8u) & 0xFFu;

    float3 p0 = MikuPanSkinPos(b0, input.aBonePos0.xyz);
    float3 p1 = MikuPanSkinPos(b1, input.aBonePos1.xyz);
    aPos = float4(p0 * w0 + p1 * w1, 1.0);

    float nw1 = 1.0 - input.aBoneNorm0.w;
    float3 n0 = MikuPanSkinNrm(b0, input.aBoneNorm0.xyz);
    float3 n1 = MikuPanSkinNrm(b1, input.aBoneNorm1.xyz);
    float normScale = uBonePalette[b0 * 4u + 3u].w;
    aNormal = float4((n0 * w0 + n1 * nw1) * normScale, 1.0);
}

VSOutput main(VSInput input)
{
    float4 aPos;
    float4 aNormal;
    MikuPanSkin(input, aPos, aNormal);

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
