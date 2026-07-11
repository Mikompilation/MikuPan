#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float2 vDstUV : TEXCOORD1;
    float4 uColor : TEXCOORD2;
};

static const float2 kSsaoKernel[12] = {
    float2( 0.5381,  0.1856),
    float2(-0.4319,  0.3227),
    float2( 0.1147, -0.6235),
    float2( 0.1851,  0.8122),
    float2(-0.7684, -0.1660),
    float2( 0.7422, -0.4372),
    float2(-0.2750, -0.7810),
    float2( 0.9144,  0.2588),
    float2(-0.9120,  0.4112),
    float2( 0.3480, -0.2160),
    float2(-0.1280,  0.5710),
    float2( 0.0520, -0.9400),
};

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453);
}

float2 Rotate(float2 v, float angle)
{
    float s = sin(angle);
    float c = cos(angle);
    return float2(v.x * c - v.y * s, v.x * s + v.y * c);
}

float SampleDepth(float2 uv)
{
    return uAuxTexture.Sample(uAuxTextureSampler, saturate(uv)).r;
}

float CalcDepthOcclusion(float2 uv, float center_depth)
{
    float2 texel = float2(1.0, 1.0) / max(uTextureSize.xy, float2(1.0, 1.0));
    float radius = max(uSsaoParams.y, 1.0);
    float bias = max(uSsaoParams.z, 0.0);
    float depth_scale = max(uSsaoParams.w, 1.0);
    float angle = Hash(floor(uv * uTextureSize.xy)) * 6.28318530718;

    float occ = 0.0;
    float total_weight = 0.0;

    [unroll]
    for (int i = 0; i < 12; i++)
    {
        float weight = 1.0 - (float)i / 12.0;
        float2 offset = Rotate(kSsaoKernel[i], angle) * texel * radius;
        float sample_depth = SampleDepth(uv + offset);

        if (sample_depth < 0.999)
        {
            float closer = center_depth - sample_depth - bias;
            float range = 1.0 - saturate(abs(center_depth - sample_depth) * 35.0);
            occ += saturate(closer * depth_scale) * range * weight;
            total_weight += weight;
        }
    }

    return total_weight > 0.0 ? occ / total_weight : 0.0;
}

float4 main(PSInput input) : SV_Target0
{
    float4 source = uTexture.Sample(uTextureSampler, input.vUV) * input.uColor;
    float center_depth = SampleDepth(input.vUV);

    if (uSsaoParams.x <= 0.001 || center_depth >= 0.999)
    {
        return source;
    }

    float occlusion = CalcDepthOcclusion(input.vUV, center_depth);
    float ao = 1.0 - saturate(occlusion * uSsaoParams.x);

    return float4(source.rgb * ao, source.a);
}
