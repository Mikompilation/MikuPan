// Untextured coloured geometry vertex shader.
// Layout: colour4 | pos4 (pre-computed NDC positions).

struct VSInput
{
    float4 aColour : TEXCOORD0;
    float4 aPos    : TEXCOORD1;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 outColor : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = input.aPos;
    output.outColor = input.aColour;
    return output;
}
