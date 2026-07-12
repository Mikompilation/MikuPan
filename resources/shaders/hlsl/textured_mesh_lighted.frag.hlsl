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

float MikuPan_Max3(float3 value)
{
    return max(max(value.x, value.y), value.z);
}

float3 MikuPan_MaterialSpecularTerm(
    float3 normal,
    float3 view_dir,
    float3 light_dir,
    float3 light_specular,
    float attenuation,
    float shininess,
    float3 f0)
{
    float ndotl = saturate(dot(normal, light_dir));
    if (ndotl <= 0.0001 || attenuation <= 0.0001)
    {
        return 0.0.xxx;
    }

    float3 half_vec = light_dir + view_dir;
    float half_len2 = dot(half_vec, half_vec);
    if (half_len2 <= 0.00001)
    {
        return 0.0.xxx;
    }
    half_vec *= rsqrt(half_len2);

    float ndoth = saturate(dot(normal, half_vec));
    float vdoth = saturate(dot(view_dir, half_vec));
    float fresnel = pow(1.0 - vdoth, 5.0);
    float3 F = f0 + (1.0.xxx - f0) * fresnel;

    float spec = pow(ndoth, shininess) * ndotl * attenuation;
    return light_specular * F * spec;
}

float3 MikuPan_CalcMaterialHighlights(
    float3 normal,
    float3 view_position,
    float3 lit_color)
{
    float enabled = saturate(uMaterialFxParams.x);
    float strength = max(uMaterialFxParams.y, 0.0) * enabled;
    if (strength <= 0.0001)
    {
        return 0.0.xxx;
    }

    float3 N = normalize(normal);
    float3 V = normalize(-view_position);
    float roughness = clamp(uMaterialFxParams.z, 0.08, 1.0);
    float shininess = lerp(96.0, 10.0, roughness);

    float3 material_specular = saturate(abs(uMatSpecular.rgb));
    float material_amount = saturate(MikuPan_Max3(material_specular));
    float3 f0 = lerp(0.035.xxx,
                     max(material_specular, 0.08.xxx),
                     0.18 + material_amount * 0.32);

    float surface_response =
        lerp(0.45, 1.0, smoothstep(0.04, 0.65, MikuPan_Max3(lit_color)));
    float3 highlight = 0.0.xxx;

    for (int i = 0; i < uParCount.x; i++)
    {
        float3 L = normalize(uParDir[i].xyz);
        highlight += MikuPan_MaterialSpecularTerm(
            N, V, L, uParSpecular[i].rgb, 1.0, shininess, f0);
    }

    for (int j = 0; j < uPointCount.x; j++)
    {
        float3 to_light = uPointPos[j].xyz - view_position;
        float dist2 = dot(to_light, to_light);
        if (dist2 <= 0.0001)
        {
            continue;
        }

        float inv_dist = rsqrt(dist2);
        float attenuation = saturate(uPointPower[j].x * inv_dist);
        highlight += MikuPan_MaterialSpecularTerm(
            N, V, to_light * inv_dist, uPointSpecular[j].rgb,
            attenuation, shininess, f0);
    }

    for (int k = 0; k < uSpotCount.x; k++)
    {
        float3 to_light = uSpotPos[k].xyz - view_position;
        float dist2 = dot(to_light, to_light);
        if (dist2 <= 0.0001)
        {
            continue;
        }

        float inv_dist = rsqrt(dist2);
        float3 L = to_light * inv_dist;
        float3 Z = normalize(uSpotDir[k].xyz);
        float cd = max(dot(L, Z), 0.0);
        float cos2 = cd * cd;
        if (cos2 <= uSpotIntens[k].x)
        {
            continue;
        }

        float gate =
            saturate((cos2 - uSpotIntens[k].x) * uSpotIntens[k].y);
        float attenuation = saturate(uSpotPower[k].x * inv_dist) * gate;
        highlight += MikuPan_MaterialSpecularTerm(
            N, V, L, uSpotSpecular[k].rgb, attenuation, shininess, f0);
    }

    highlight = ApplyBlackWhiteLightOnly(highlight);
    return highlight * strength * surface_response;
}

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
    color.rgb += MikuPan_CalcMaterialHighlights(
        input.vNormal.xyz, input.oViewPosition.xyz, color.rgb);

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
