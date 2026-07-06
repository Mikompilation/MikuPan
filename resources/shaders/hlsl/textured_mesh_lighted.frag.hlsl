#include "mikupan_common.hlsli"

#define MIKUPAN_PS2_SCENE_BLACK_POINT      0.004
#define MIKUPAN_PS2_SCENE_CONTRAST         1.08
#define MIKUPAN_PS2_SCENE_PIVOT            0.22
#define MIKUPAN_PS2_SCENE_HIGHLIGHT_START  0.62
#define MIKUPAN_PS2_SCENE_HIGHLIGHT_GAIN   0.88
#define MIKUPAN_PS2_SCENE_EXTRA_POWER      0.45

float3 MikuPan_ApplyPs2SceneToneBase(float3 color)
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

float3 MikuPan_ApplyPs2SceneTone(float3 color)
{
    float strength = clamp(uParams1.w, 0.0, 2.0);
    float3 neutral = saturate(color);
    float3 ps2 = MikuPan_ApplyPs2SceneToneBase(neutral);

    if (strength <= 1.0)
    {
        return saturate(lerp(neutral, ps2, strength));
    }

    float extra = saturate(strength - 1.0);
    float3 shadowWeight = 1.0 - smoothstep(0.10.xxx, 0.55.xxx, ps2);
    float3 deeper = pow(max(ps2, 0.0.xxx), (1.0 + MIKUPAN_PS2_SCENE_EXTRA_POWER * extra).xxx);
    return saturate(lerp(ps2, deeper, shadowWeight * extra));
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

    if (uFlags1.x == 2)
    {
        if (color.a <= (1.0 / 255.0))
        {
            discard;
        }

        float2 mirrorUv = saturate(input.position.xy / max(uRenderSize.xy, 1.0.xx));
        float4 reflection = uAuxTexture.Sample(uAuxTextureSampler, mirrorUv);
        return float4(reflection.rgb, 1.0);
    }

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
                float occluded = uAuxTexture.Sample(uAuxTextureSampler, suv).r;
                color.rgb *= 1.0 - occluded * uParams0.y;
            }
        }
    }

    color.rgb = MikuPan_ApplyPs2SceneTone(color.rgb);

    return color;
}
