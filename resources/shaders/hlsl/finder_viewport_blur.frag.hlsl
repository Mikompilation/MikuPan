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

float3 HorrorLensBlur(float2 uv, float2 texel)
{
    float2 center = 0.5.xx;
    float2 dir = uv - center;

    float dist = length(dir * float2(1.0, 0.82));

    /*
        Stronger blur toward the outside of the lens.
        Center remains more readable.
    */
    float edge_blur = smoothstep(0.10, 0.72, dist);

    float3 c = 0.0.xxx;
    float total = 0.0;

    float w;

    w = 0.28;
    c += SampleScene(uv) * w;
    total += w;

    w = 0.20 * edge_blur;
    c += SampleScene(uv - dir * 0.025) * w;
    total += w;

    w = 0.16 * edge_blur;
    c += SampleScene(uv - dir * 0.055) * w;
    total += w;

    w = 0.12 * edge_blur;
    c += SampleScene(uv - dir * 0.090) * w;
    total += w;

    w = 0.09 * edge_blur;
    c += SampleScene(uv - dir * 0.135) * w;
    total += w;

    w = 0.06 * edge_blur;
    c += SampleScene(uv - dir * 0.190) * w;
    total += w;

    /*
        Small asymmetric ghost offsets.
        These make the image feel less clean and more haunted.
    */
    w = 0.055 * edge_blur;
    c += SampleScene(uv + texel * float2(4.0, -2.0)) * w;
    total += w;

    w = 0.045 * edge_blur;
    c += SampleScene(uv + texel * float2(-5.0, 3.0)) * w;
    total += w;

    return c / max(total, 0.000001);
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

    uv = clamp(uv + input.vUVData.xy * 0.000001, 0.0.xx, 1.0.xx);

    float2 texel = 1.0.xx / max(uRenderSize.xy, 1.0.xx);

    float3 blur = HorrorLensBlur(uv, texel);

    /*
        Lens mask.
        1.0 near the center, 0.0 near the outer edge.
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

    /*
        Slight desaturation helps make it feel older and colder.
    */
    float luma = dot(blur, float3(0.299, 0.587, 0.114));
    blur = lerp(luma.xxx, blur, 0.72);

    float3 out_rgb = blur;

    /*
        Darken outer lens area while keeping the center visible.
    */
    out_rgb *= lerp(0.34, 0.78, lens_vignette);

    /*
        Purple/cold tint.
    */
    out_rgb *= lens_tint;

    /*
        Optional extra edge darkness.
        This gives more of a camera-objective look.
    */
    float edge_darkness = smoothstep(0.18, 0.95, lens_vignette);
    out_rgb *= lerp(0.72, 1.0, edge_darkness);

    return float4(out_rgb, fade);
}