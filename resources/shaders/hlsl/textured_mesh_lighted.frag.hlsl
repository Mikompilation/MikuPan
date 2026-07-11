#include "mikupan_common.hlsli"

/* Scene-only PS2 presentation compensation. This is intentionally a
 * mild contrast/black-point test, not a gamma curve: the original hardware
 * image crushes dark scene detail, but the brightest native 3D highlights
 * should not blow out as hard as clean PC output. Keep this in the native
 * 3D mesh shader for now so HUD/menu sprites are not affected. */
#define MIKUPAN_PS2_SCENE_BLACK_POINT      0.004
#define MIKUPAN_PS2_SCENE_CONTRAST         1.08
#define MIKUPAN_PS2_SCENE_PIVOT            0.22
#define MIKUPAN_PS2_SCENE_HIGHLIGHT_START  0.62
#define MIKUPAN_PS2_SCENE_HIGHLIGHT_GAIN   0.88

float3 MikuPan_ApplyPs2SceneTone(float3 color)
{
    color = saturate(color);
    color = saturate((color - MIKUPAN_PS2_SCENE_BLACK_POINT) /
                     (1.0 - MIKUPAN_PS2_SCENE_BLACK_POINT));
    color = saturate((color - MIKUPAN_PS2_SCENE_PIVOT) *
                     MIKUPAN_PS2_SCENE_CONTRAST +
                     MIKUPAN_PS2_SCENE_PIVOT);

    float3 highlightWeight = saturate((color - MIKUPAN_PS2_SCENE_HIGHLIGHT_START) /
                                      (1.0 - MIKUPAN_PS2_SCENE_HIGHLIGHT_START));
    float3 rolledHighlights = MIKUPAN_PS2_SCENE_HIGHLIGHT_START +
                              (color - MIKUPAN_PS2_SCENE_HIGHLIGHT_START) *
                              MIKUPAN_PS2_SCENE_HIGHLIGHT_GAIN;
    return saturate(lerp(color, rolledHighlights, highlightWeight));
}

static const float2 kShadowPcfDisk[12] = {
    float2( 0.000,  0.500),
    float2( 0.433,  0.250),
    float2( 0.433, -0.250),
    float2( 0.000, -0.500),
    float2(-0.433, -0.250),
    float2(-0.433,  0.250),
    float2( 0.780,  0.000),
    float2( 0.390,  0.676),
    float2(-0.390,  0.676),
    float2(-0.780,  0.000),
    float2(-0.390, -0.676),
    float2( 0.390, -0.676),
};

float MikuPan_SampleShadowMask(float2 uv)
{
    return uAuxTexture.Sample(uAuxTextureSampler, saturate(uv)).r;
}

float MikuPan_SampleShadowOcclusion(float2 uv)
{
    float radius = max(uShadowSize.z, 0.0);
    if (uShadowSize.w <= 0.5 || radius <= 0.01)
    {
        return MikuPan_SampleShadowMask(uv);
    }

    float2 texel_radius = radius / max(uShadowSize.xy, 1.0.xx);
    float occlusion = MikuPan_SampleShadowMask(uv) * 2.0;
    float total_weight = 2.0;

    [unroll]
    for (int i = 0; i < 12; i++)
    {
        float tap_weight = 1.0 - saturate(length(kShadowPcfDisk[i]) * 0.35);
        occlusion +=
            MikuPan_SampleShadowMask(uv + kShadowPcfDisk[i] * texel_radius) *
            tap_weight;
        total_weight += tap_weight;
    }

    return occlusion / total_weight;
}

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float4 vNormal : TEXCOORD1;
    float4 oViewPosition : TEXCOORD2;
    float4 oWorldPosition : TEXCOORD3;
    float3 oVertexColor : TEXCOORD4;
    float3 oLitVertexColor : TEXCOORD5;
};

float4 main(PSInput input) : SV_Target0
{
    float4 color = uTexture.Sample(uTextureSampler, input.vUV);

    float materialAlpha = clamp(uMaterialAlpha.x, 0.0, 1.0);
    color.a *= materialAlpha;

    // Shadow caster pass. The original GS shadow setup alpha-tests caster
    // fragments, so paper/cloth cutouts must punch holes in the silhouette
    // texture instead of casting a filled quad. uShadowEnabled == 2 is used
    // only while MikuPan_BeginShadowPass is drawing caster meshes.
    if (uFlags1.y == 2)
    {
        if (color.a <= (1.0 / 255.0))
        {
            discard;
        }

        return 1.0.xxxx;
    }

    if (uFlags0.x == 1)
    {
        return input.vNormal;
    }

    if (color.a <= (1.0 / 255.0))
    {
        discard;
    }

    if (uFlags0.z == 1)
    {
        return float4(MikuPan_ApplyPs2SceneTone(input.oVertexColor), color.a);
    }

    if (uFlags0.y == 1)
    {
        color.rgb = MikuPan_ApplyPs2SceneTone(color.rgb);
        return color;
    }

    float3 ps2LightColor = uFlags0.w == 1
        ? input.oLitVertexColor
        : CalcPS2LitColor(input.vNormal, input.oViewPosition, input.oVertexColor);
    ps2LightColor = ApplyBlackWhiteLightOnly(ps2LightColor);
    color.rgb = ApplyGsModulate(color.rgb, ps2LightColor);

    if (uFlags1.x == 0)
    {
        float fogFactor = uFog.w * (1.0 / -input.oViewPosition.z) + uFog.z;
        fogFactor = clamp(fogFactor, uFog.x, uFog.y);
        color.rgb = lerp(uFogColor.rgb, color.rgb, fogFactor);
    }

    if (uFlags1.y == 1)
    {
        float4 shadowClip = mul(uShadowMatrix, input.oWorldPosition);
        if (shadowClip.w > 0.0)
        {
            float3 sndc = shadowClip.xyz / shadowClip.w;
            // Flip y: offscreen targets are top-down here (see shadow_receiver.frag).
            float2 suv = float2(sndc.x * 0.5 + 0.5, 0.5 - sndc.y * 0.5);
            if (suv.x >= 0.0 && suv.x <= 1.0 &&
                suv.y >= 0.0 && suv.y <= 1.0 &&
                sndc.z >= -1.0 && sndc.z <= 1.0)
            {
                float occluded = MikuPan_SampleShadowOcclusion(suv);
                color.rgb *= 1.0 - occluded * uParams0.y;
            }
        }
    }

    color.rgb = MikuPan_ApplyPs2SceneTone(color.rgb);

    return color;
}
