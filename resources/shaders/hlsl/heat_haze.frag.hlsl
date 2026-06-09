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

    if (uFlags2.w == 1)
    {
        float2 screen_uv = clamp(float2(input.position.x / uRenderSize.x,
                                        1.0 - input.position.y / uRenderSize.y),
                                 0.0.xx, 1.0.xx);
        src_uv = screen_uv;
        dst_uv = screen_uv;
    }
    else if (uFlags2.w == 2)
    {
        float2 screen_uv = clamp(float2(input.position.x / uRenderSize.x,
                                        1.0 - input.position.y / uRenderSize.y),
                                 0.0.xx, 1.0.xx);
        src_uv = clamp(input.vUVData.xy, 0.0.xx, 1.0.xx);
        dst_uv = screen_uv;
        resolve_deform = true;
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

    // The screen-copy bound here is a straight copy of the scene render target,
    // which SDL_GPU stores top-down (V=0 == top row). The framebuffer UVs above
    // (and the 1.0 - position.y screen_uv branches) were authored for the PS2/GL
    // bottom-up framebuffer, so flip V to land on the matching texel. Letterbox
    // bars are symmetric, so mirroring across the full V range keeps the sample
    // inside the scene content region.
    src_uv.y = 1.0 - src_uv.y;
    dst_uv.y = 1.0 - dst_uv.y;

    float3 src = uTexture.Sample(uTextureSampler, src_uv).rgb;
    float3 dst = uTexture.Sample(uTextureSampler, dst_uv).rgb;

    float color_strength = max(max(input.uColor.r, input.uColor.g), input.uColor.b);
    float refract_strength = clamp(color_strength, 0.0, 1.0);
    float3 refracted = lerp(dst, src, refract_strength);
    float3 out_rgb = resolve_deform ? lerp(dst, refracted, mask) : refracted;

    if (uFlags2.x != 0)
    {
        out_rgb = ToBlackWhite(out_rgb);
    }

    return float4(out_rgb, resolve_deform ? 1.0 : mask);
}
