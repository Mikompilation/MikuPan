#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float4 vShadowClip : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    if (input.vShadowClip.w <= 0.0)
    {
        if (uFlags1.z != 0)
        {
            return float4(0.0, 0.0, 0.45, 1.0);
        }
        discard;
    }

    float3 sndc = input.vShadowClip.xyz / input.vShadowClip.w;
    // Offscreen render targets are stored top-down in this backend (cglm NDC
    // y=+1 maps to texture v=0; see heat_haze.frag). The silhouette caster is
    // rasterized with the same cglm matrix, so flip y when sampling or the shadow
    // is mirrored vertically and lands off its caster.
    float2 suv = float2(sndc.x * 0.5 + 0.5, 0.5 - sndc.y * 0.5);

    // The R8 shadow map stores coverage only (no depth), so occlusion is decided
    // purely by XY footprint inside the projector frustum. A z-range test here
    // would wrongly drop shadows on surfaces outside the light's depth slab.
    if (suv.x < 0.0 || suv.x > 1.0 ||
        suv.y < 0.0 || suv.y > 1.0)
    {
        if (uFlags1.z != 0)
        {
            return float4(0.0, 0.0, 0.45, 1.0);
        }
        discard;
    }

    float occluded = uAuxTexture.Sample(uAuxTextureSampler, float2(suv.x, 1.0 - suv.y)).r;
    if (uFlags1.z != 0)
    {
        return float4(occluded, 0.35 + 0.65 * (1.0 - occluded), 0.0, 1.0);
    }

    float alpha = occluded * uParams0.y;
    if (alpha <= 0.001)
    {
        discard;
    }

    return float4(0.0, 0.0, 0.0, alpha);
}
