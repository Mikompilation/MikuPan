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

float4 main(PSInput input) : SV_Target0
{
    float fade = saturate(input.uColor.a);
    if (fade <= (1.0 / 255.0))
    {
        discard;
    }

    float2 uv = clamp(float2(input.position.x / max(uRenderSize.x, 1.0),
                             input.position.y / max(uRenderSize.y, 1.0)),
                      0.0.xx, 1.0.xx);
    uv = clamp(uv + input.vUVData.xy * 0.000001, 0.0.xx, 1.0.xx);
    float2 texel = 1.0.xx / max(uRenderSize.xy, 1.0.xx);

    float3 blur = SampleScene(uv) * 0.080;
    blur += SampleScene(uv + texel * float2( 14.0,   0.0)) * 0.060;
    blur += SampleScene(uv + texel * float2(-14.0,   0.0)) * 0.060;
    blur += SampleScene(uv + texel * float2(  0.0,  14.0)) * 0.060;
    blur += SampleScene(uv + texel * float2(  0.0, -14.0)) * 0.060;
    blur += SampleScene(uv + texel * float2( 25.0,  15.0)) * 0.050;
    blur += SampleScene(uv + texel * float2(-25.0, -15.0)) * 0.050;
    blur += SampleScene(uv + texel * float2(-25.0,  15.0)) * 0.050;
    blur += SampleScene(uv + texel * float2( 25.0, -15.0)) * 0.050;
    blur += SampleScene(uv + texel * float2( 43.0,   0.0)) * 0.042;
    blur += SampleScene(uv + texel * float2(-43.0,   0.0)) * 0.042;
    blur += SampleScene(uv + texel * float2(  0.0,  43.0)) * 0.042;
    blur += SampleScene(uv + texel * float2(  0.0, -43.0)) * 0.042;
    blur += SampleScene(uv + texel * float2( 42.0,  35.0)) * 0.035;
    blur += SampleScene(uv + texel * float2(-42.0, -35.0)) * 0.035;
    blur += SampleScene(uv + texel * float2(-42.0,  35.0)) * 0.035;
    blur += SampleScene(uv + texel * float2( 42.0, -35.0)) * 0.035;
    blur += SampleScene(uv + texel * float2( 66.0,  20.0)) * 0.027;
    blur += SampleScene(uv + texel * float2(-66.0, -20.0)) * 0.027;
    blur += SampleScene(uv + texel * float2(-66.0,  20.0)) * 0.027;
    blur += SampleScene(uv + texel * float2( 66.0, -20.0)) * 0.027;

    float lens_vignette = smoothstep(0.88, 0.22, length((uv - 0.5.xx) * float2(1.0, 0.82)));
    float3 lens_tint = float3(0.72, 0.67, 0.82);
    float3 out_rgb = blur * lerp(0.42, 0.74, lens_vignette) * lens_tint;

    return float4(out_rgb, fade);
}
