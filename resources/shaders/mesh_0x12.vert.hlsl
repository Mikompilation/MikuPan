// mesh_0x12: pos3 + norm3 (interleaved, stride 24) | uv2 (stride 8)
// Applies model + view + projection transforms.

cbuffer MeshUniforms : register(b0, space1)
{
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 projection;
};

struct VSInput
{
    float3 aPos    : TEXCOORD0;
    float3 aNormal : TEXCOORD1;
    float2 aUV     : TEXCOORD2;
};

struct VSOutput
{
    float4 Position      : SV_Position;
    float2 vUV           : TEXCOORD0;
    float4 vNormal       : TEXCOORD1;
    float4 oViewPosition : TEXCOORD2;
};

// Returns transpose(inverse(m)) — the correct matrix for transforming normals.
float3x3 NormalMatrix(float3x3 m)
{
    float det = determinant(m);
    float3x3 adj;
    adj[0][0] =  (m[1][1]*m[2][2] - m[2][1]*m[1][2]);
    adj[0][1] = -(m[0][1]*m[2][2] - m[2][1]*m[0][2]);
    adj[0][2] =  (m[0][1]*m[1][2] - m[1][1]*m[0][2]);
    adj[1][0] = -(m[1][0]*m[2][2] - m[2][0]*m[1][2]);
    adj[1][1] =  (m[0][0]*m[2][2] - m[2][0]*m[0][2]);
    adj[1][2] = -(m[0][0]*m[1][2] - m[1][0]*m[0][2]);
    adj[2][0] =  (m[1][0]*m[2][1] - m[2][0]*m[1][1]);
    adj[2][1] = -(m[0][0]*m[2][1] - m[2][0]*m[0][1]);
    adj[2][2] =  (m[0][0]*m[1][1] - m[1][0]*m[0][1]);
    return transpose(adj) / det;
}

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 pos4  = float4(input.aPos, 1.0);
    float4x4 mv  = mul(view, model);

    output.vUV           = input.aUV;
    output.Position      = mul(projection, mul(mv, pos4));
    output.oViewPosition = mul(mv, pos4);

    float3x3 normalMat = NormalMatrix((float3x3)mv);
    output.vNormal = float4(normalize(mul(normalMat, input.aNormal)), 1.0);

    return output;
}
