#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float3 vViewPos : TEXCOORD0;
};

float Hash(float3 p)
{
    return frac(sin(dot(p, float3(17.17, 41.41, 113.13))) * 43758.5453);
}

float4 main(PSInput input) : SV_Target0
{
    if (uVolumetricLight.w <= 0.001 || uVolumetricParams.x <= 0.001)
    {
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float3 source = uVolumetricLight.xyz;
    float3 axis = normalize(uVolumetricBeam.xyz);
    float range = max(uVolumetricBeam.w, 1.0);
    float far_radius = max(uVolumetricParams.y, 1.0);
    float density = max(uVolumetricParams.z, 0.0);
    float slice_scale = max(uVolumetricParams.w, 0.001);

    float3 rel = input.vViewPos - source;
    float axial = dot(rel, axis);
    if (axial <= 0.0 || axial >= range)
    {
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float3 lateral_vec = rel - axis * axial;
    float radius = max(far_radius * saturate(axial / range), 1.0);
    float radial01 = length(lateral_vec) / radius;
    if (radial01 >= 1.0)
    {
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float radial_fade = 1.0 - smoothstep(0.18, 1.0, radial01);
    radial_fade *= radial_fade;

    float near_fade = smoothstep(range * 0.015, range * 0.10, axial);
    float far_fade = 1.0 - smoothstep(range * 0.72, range, axial);
    float center_weight = 1.0 - smoothstep(0.0, range, abs(axial - range * 0.36));
    float axial_fade = near_fade * far_fade * lerp(0.55, 1.0, center_weight);

    float3 noise_pos = input.vViewPos * 0.006 +
                       float3(uParams1.x * 0.10, uParams1.x * 0.04, 0.0);
    float noise = lerp(0.86, 1.10, Hash(floor(noise_pos)));

    float strength = saturate(uVolumetricParams.x);
    float alpha = strength * density * slice_scale *
                  radial_fade * axial_fade * noise;
    alpha = saturate(alpha);
    if (alpha <= 0.001)
    {
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float3 tint =
        lerp(saturate(uFogColor.rgb), uVolumetricColor.rgb,
             saturate(uVolumetricColor.a));
    float warm_core = 1.0 - smoothstep(0.0, 0.55, radial01);
    tint = lerp(tint, uVolumetricColor.rgb, warm_core * 0.35);

    return float4(saturate(tint), alpha);
}
