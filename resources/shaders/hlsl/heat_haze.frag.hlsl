#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float4 vUVData : TEXCOORD0;
    float4 uColor : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target0
{
    float mask = clamp(input.uColor.a, 0.0, 1.0);
    if (mask <= (1.0 / 255.0))
    {
        discard;
    }

    float2 src_uv;
    float2 dst_uv;
    bool resolve_deform = false;
    bool feedback_modulate = false;
    bool screen_pos_modulate = false;

    if (uFlags2.w == 1)
    {
        // The screen-copy is stored top-down, and SV_Position.y is already
        // top-down, so sample with position.y directly. Using 1.0 - position.y
        // mirrors it vertically and composites a flipped full-screen copy of
        // the scene (the "double flipped screen effect").
        float2 screen_uv = clamp(float2(input.position.x / uRenderSize.x,
                                        input.position.y / uRenderSize.y),
                                 0.0.xx, 1.0.xx);
        src_uv = screen_uv;
        dst_uv = screen_uv;
    }
    else if (uFlags2.w == 2)
    {
        // The screen-copy is stored top-down, and SV_Position.y is already
        // top-down, so sample with position.y directly. Using 1.0 - position.y
        // mirrors it vertically and composites a flipped full-screen copy of
        // the scene (the "double flipped screen effect").
        float2 screen_uv = clamp(float2(input.position.x / uRenderSize.x,
                                        input.position.y / uRenderSize.y),
                                 0.0.xx, 1.0.xx);
        src_uv = clamp(input.vUVData.xy, 0.0.xx, 1.0.xx);
        dst_uv = screen_uv;
        resolve_deform = true;
    }
    else if (uFlags2.w == 3)
    {
        src_uv = clamp(
            uFramebufferUvOffset.xy + input.vUVData.xy * uFramebufferUvScale.xy,
            uFramebufferUvOffset.xy,
            uFramebufferContentUvMax.xy);
        dst_uv = src_uv;
        feedback_modulate = true;
    }
    else if (uFlags2.w == 4)
    {
        float2 screen_uv = clamp(float2(input.position.x / uRenderSize.x,
                                        input.position.y / uRenderSize.y),
                                 0.0.xx, 1.0.xx);
        src_uv = screen_uv;
        dst_uv = screen_uv;
        screen_pos_modulate = true;
    }
    else
    {
        src_uv = clamp(
            uFramebufferUvOffset.xy + input.vUVData.xy * uFramebufferUvScale.xy,
            uFramebufferUvOffset.xy,
            uFramebufferContentUvMax.xy);
        dst_uv = clamp(
            uFramebufferUvOffset.xy + input.vUVData.zw * uFramebufferUvScale.xy,
            uFramebufferUvOffset.xy,
            uFramebufferContentUvMax.xy);
    }

    float3 src = uTexture.Sample(uTextureSampler, src_uv).rgb;
    float3 dst = uTexture.Sample(uTextureSampler, dst_uv).rgb;

    float3 out_rgb;

    if (feedback_modulate || screen_pos_modulate)
    {
        out_rgb = src * input.uColor.rgb;
    }
    else
    {
        float color_strength = max(max(input.uColor.r, input.uColor.g), input.uColor.b);
        float refract_strength = clamp(color_strength, 0.0, 1.0);
        float3 refracted = lerp(dst, src, refract_strength);
        out_rgb = resolve_deform ? lerp(dst, refracted, mask) : refracted;
    }

    if (uFlags2.x != 0)
    {
        out_rgb = ToBlackWhite(out_rgb);
    }

    return float4(out_rgb, resolve_deform ? 1.0 : mask);
}
