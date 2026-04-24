// Untextured coloured geometry fragment shader — passthrough vertex colour.

struct PSInput
{
    float4 Position : SV_Position;
    float4 outColor : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return input.outColor;
}
