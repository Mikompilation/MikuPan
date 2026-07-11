#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float2 vDstUV : TEXCOORD1;
    float4 uColor : TEXCOORD2;
};

static const int kShaftSamples = 20;

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(41.131, 289.97))) * 43758.5453);
}

float SampleDepth(float2 uv)
{
    return uAuxTexture.Sample(uAuxTextureSampler, saturate(uv)).r;
}

float InBounds(float2 uv)
{
    float2 lower = step(float2(0.0, 0.0), uv);
    float2 upper = step(uv, float2(1.0, 1.0));
    return lower.x * lower.y * upper.x * upper.y;
}

float4 main(PSInput input) : SV_Target0
{
    float4 source = uTexture.Sample(uTextureSampler, input.vUV) * input.uColor;

    if (uVolumetricLight.w <= 0.001 || uVolumetricParams.x <= 0.001)
    {
        return source;
    }

    float2 texture_size = max(uTextureSize.xy, float2(1.0, 1.0));
    float aspect = texture_size.x / texture_size.y;
    float2 source_uv = uVolumetricLight.xy;
    float2 uv_to_source = source_uv - input.vUV;
    float2 aspect_delta = float2(uv_to_source.x * aspect, uv_to_source.y);
    float radial_dist = length(aspect_delta);
    float radius = max(uVolumetricParams.y, 0.01);
    float cone_mask = 1.0 - smoothstep(radius * 0.18, radius, radial_dist);

    if (cone_mask <= 0.001)
    {
        return source;
    }

    float center_depth = SampleDepth(input.vUV);
    float2 step_uv = uv_to_source * (max(uVolumetricParams.z, 0.0) /
                                     (float)kShaftSamples);
    float decay = saturate(uVolumetricParams.w);
    float illumination = 0.0;
    float weight = 1.0;
    float total_weight = 0.0;
    float2 sample_uv = input.vUV;

    [unroll]
    for (int i = 0; i < kShaftSamples; i++)
    {
        sample_uv += step_uv;

        float in_bounds = InBounds(sample_uv);
        float sample_depth = SampleDepth(sample_uv);
        float far_medium = smoothstep(0.18, 0.98, sample_depth);
        float unoccluded = smoothstep(-0.012, 0.045,
                                      sample_depth - center_depth);
        float source_visibility =
            smoothstep(-0.10, 0.25, sample_depth - uVolumetricLight.z);

        illumination +=
            in_bounds * far_medium * unoccluded * source_visibility * weight;
        total_weight += in_bounds * weight;
        weight *= decay;
    }

    illumination = total_weight > 0.0 ? illumination / total_weight : 0.0;

    float edge_noise =
        Hash(floor(input.vUV * texture_size * 0.18) +
             float2(uParams1.x * 19.0, uParams1.x * 7.0));
    float shimmer = lerp(0.88, 1.08, edge_noise);
    float strength = saturate(uVolumetricParams.x) * uVolumetricLight.w;
    float shaft = illumination * cone_mask * strength * shimmer;

    float3 shaft_tint =
        lerp(uFogColor.rgb, uVolumetricColor.rgb, saturate(uVolumetricColor.a));
    float headroom =
        1.0 - max(max(source.r, source.g), source.b) * 0.35;
    float3 color = source.rgb + shaft_tint * shaft * max(headroom, 0.25);

    return float4(saturate(color), source.a);
}
