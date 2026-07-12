#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float2 vDstUV : TEXCOORD1;
    float4 uColor : TEXCOORD2;
};

static const float2 kBloomKernel[16] = {
    float2( 0.7071,  0.0000),
    float2(-0.7071,  0.0000),
    float2( 0.0000,  0.7071),
    float2( 0.0000, -0.7071),
    float2( 0.5000,  0.5000),
    float2(-0.5000,  0.5000),
    float2( 0.5000, -0.5000),
    float2(-0.5000, -0.5000),
    float2( 1.0000,  0.3827),
    float2(-1.0000,  0.3827),
    float2( 1.0000, -0.3827),
    float2(-1.0000, -0.3827),
    float2( 0.3827,  1.0000),
    float2(-0.3827,  1.0000),
    float2( 0.3827, -1.0000),
    float2(-0.3827, -1.0000),
};

float Luma(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float2 ClampBloomUv(float2 uv)
{
    float2 texel = 1.0 / max(uTextureSize.xy, 1.0.xx);
    return clamp(uv, texel * 0.5, 1.0.xx - texel * 0.5);
}

float3 SampleScene(float2 uv)
{
    return uTexture.Sample(uTextureSampler, ClampBloomUv(uv)).rgb * uColor.rgb;
}

float3 ExtractBloom(float3 color)
{
    float brightness = Luma(color);
    float threshold = saturate(uBloomParams.y);
    float knee = max(uBloomParams.w, 0.001);
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.001);
    return color * saturate(contribution);
}

float4 main(PSInput input) : SV_Target0
{
    float4 source = uTexture.Sample(uTextureSampler, input.vUV) * input.uColor;
    float strength = max(uBloomParams.x, 0.0);
    if (strength <= 0.001)
    {
        return source;
    }

    float2 texel = 1.0 / max(uTextureSize.xy, 1.0.xx);
    float radius = max(uBloomParams.z, 0.5);
    float3 bloom = ExtractBloom(SampleScene(input.vUV)) * 1.2;
    float total_weight = 1.2;

    [unroll]
    for (int i = 0; i < 16; i++)
    {
        float2 kernel = kBloomKernel[i];
        float spread = 0.45 + (float)(i % 4) * 0.28;
        float2 uv = input.vUV + kernel * texel * radius * spread;
        float weight = 1.0 / (1.0 + dot(kernel, kernel) * 0.65 + spread * 0.20);
        bloom += ExtractBloom(SampleScene(uv)) * weight;
        total_weight += weight;
    }

    bloom /= max(total_weight, 0.001);
    float3 color = source.rgb + bloom * strength;
    return float4(saturate(color), source.a);
}
