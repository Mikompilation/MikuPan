#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float4 vUVData : TEXCOORD0;
    float4 uColor : TEXCOORD1;
};

float3 SampleScene(float2 uv)
{
    return uTexture.Sample(uTextureSampler, clamp(uv, 0.0.xx, 1.0.xx)).rgb;
}

float GaussianWeight(float x, float sigma)
{
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

float4 main(PSInput input) : SV_Target0
{
    float fade = saturate(input.uColor.a);

    if (fade <= (1.0 / 255.0))
    {
        discard;
    }

    float2 uv = clamp(
        float2(
            input.position.x / max(uRenderSize.x, 1.0),
            input.position.y / max(uRenderSize.y, 1.0)
        ),
        0.0.xx,
        1.0.xx
    );

    // Keeps the UV data dependency from your original shader.
    uv = clamp(uv + input.vUVData.xy * 0.000001, 0.0.xx, 1.0.xx);

    float2 texel = 1.0.xx / max(uRenderSize.xy, 1.0.xx);

    /*
        Gaussian blur settings.

        blur_radius controls how far the blur reaches in pixels.
        sigma controls how soft/wide the Gaussian curve is.

        Larger radius = wider blur, more expensive.
        Larger sigma  = softer blur distribution.
    */
    const float blur_radius = 6.0;
    const float sigma = 6.0;
    const int blur_sample = 6;

    float3 blur = 0.0.xxx;
    float total_weight = 0.0;

    /*
        9x9 Gaussian kernel.

        This is a proper 2D Gaussian blur.
        You can increase the loop range to -6..6 for a heavier blur,
        but it will cost more samples.
    */
    
    [unroll]
    for (int y = -blur_sample; y <= blur_sample; y++)
    {
        [unroll]
        for (int x = -blur_sample; x <= blur_sample; x++)
        {
            float2 offset = float2((float) x, (float) y);

            float dist = length(offset);
            float weight = GaussianWeight(dist, sigma);

            float2 sample_uv = uv + offset * texel * blur_radius;

            blur += SampleScene(sample_uv) * weight;
            total_weight += weight;
        }
    }

    blur /= max(total_weight, 0.000001);

    /*
        Lens/vignette mask from your original shader.

        The center remains brighter, the outside gets darker.
    */
    float lens_vignette = smoothstep(
        0.88,
        0.22,
        length((uv - 0.5.xx) * float2(1.0, 0.82))
    );

    /*
        Existing tint from your shader.
    */
    float3 lens_tint = float3(0.72, 0.67, 0.82);

    float3 out_rgb = blur;

    // Apply the same general brightness shaping as your original shader.
    out_rgb *= lerp(0.42, 0.74, lens_vignette);

    // Apply the color tint.
    out_rgb *= lens_tint;

    return float4(out_rgb, fade);
}