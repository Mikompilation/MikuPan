#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float2 vDstUV : TEXCOORD1;
    float4 uColor : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target0
{
    float4 texel = uTexture.Sample(uTextureSampler, input.vUV);

    if (uPadFlags.z != 0)
    {
        float2 texel_size = 1.0 / max(uTextureSize.xy, 1.0.xx);
        float4 soft = texel * 0.40;
        soft += uTexture.Sample(uTextureSampler, input.vUV + float2( texel_size.x, 0.0)) * 0.15;
        soft += uTexture.Sample(uTextureSampler, input.vUV + float2(-texel_size.x, 0.0)) * 0.15;
        soft += uTexture.Sample(uTextureSampler, input.vUV + float2(0.0,  texel_size.y)) * 0.15;
        soft += uTexture.Sample(uTextureSampler, input.vUV + float2(0.0, -texel_size.y)) * 0.15;
        texel = soft;
    }

    float4 col = texel * input.uColor;

    if (uParams1.y > 0.001)
    {
        float strength = clamp(uParams1.y, 0.0, 1.0);
        float3 negative_color = clamp((96.0 / 255.0).xxx - col.rgb, 0.0.xxx, 1.0.xxx);
        col.rgb = lerp(col.rgb, negative_color, strength);
    }

    if (col.a <= 0.0)
    {
        discard;
    }

    return col;
}
