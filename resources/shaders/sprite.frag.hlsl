// Sprite fragment shader — samples texture and multiplies by vertex colour.

[[vk::combinedImageSampler]]
Texture2D    uTexture : register(t0, space2);
[[vk::combinedImageSampler]]
SamplerState uSampler : register(s0, space2);

struct PSInput
{
    float4 Position : SV_Position;
    float2 vUV      : TEXCOORD0;
    float4 uColor   : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target
{
    return uTexture.Sample(uSampler, input.vUV) * input.uColor;
}
