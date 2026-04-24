// Sprite vertex shader.
// Layout: uv4 | colour4 | pos4 (all pre-computed on CPU, pos already in NDC).

struct VSInput
{
    float4 aUV      : TEXCOORD0;
    float4 inColor  : TEXCOORD1;
    float4 aPos     : TEXCOORD2;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float2 vUV      : TEXCOORD0;
    float4 uColor   : TEXCOORD1;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = input.aPos;          // already in NDC
    output.vUV      = input.aUV.xy;
    output.uColor   = input.inColor;
    return output;
}
