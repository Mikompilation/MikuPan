// Bounding box line strip vertex shader.
// Applies model + view + projection and passes through the box colour.

cbuffer BBUniforms : register(b0, space1)
{
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 projection;
    float4 uColor;
};

struct VSInput
{
    float4 aPos : TEXCOORD0;
};

struct VSOutput
{
    float4 Position : SV_Position;
    float4 outColor : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(projection, mul(view, mul(model, input.aPos)));
    output.outColor = uColor;
    return output;
}
