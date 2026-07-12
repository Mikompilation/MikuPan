#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float3 vViewPos : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(19.19, 73.31))) * 43758.5453);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = Hash(i);
    float b = Hash(i + float2(1.0, 0.0));
    float c = Hash(i + float2(0.0, 1.0));
    float d = Hash(i + float2(1.0, 1.0));

    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float3 AtmosphericFogTint(void)
{
    float3 room_fog = saturate(uFogColor.rgb);
    float3 cold_air = float3(0.11, 0.15, 0.17);
    float room_luma = dot(room_fog, float3(0.2126, 0.7152, 0.0722));
    float3 room_tint = lerp(cold_air, room_fog, 0.35);

    return lerp(room_tint * 0.72, room_tint * 1.12,
                smoothstep(0.04, 0.62, room_luma));
}

float4 main(PSInput input) : SV_Target0
{
    if (uAtmosphericFogParams.w <= 0.5 || uAtmosphericFogParams.x <= 0.001)
    {
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float strength = saturate(uAtmosphericFogParams.x);
    float density = max(uAtmosphericFogParams.y, 0.0);
    float height_bias = saturate(uAtmosphericFogParams.z);

    float distance = max(-input.vViewPos.z, 0.0);
    if (distance <= 1.0)
    {
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float near_fade = smoothstep(140.0, 520.0, distance);
    float far_fade = 1.0 - smoothstep(5200.0, 7200.0, distance);
    float distance_fade = near_fade * far_fade;

    float view_slope = input.vViewPos.y / max(distance, 1.0);
    float lower_air = 1.0 - smoothstep(0.08 + height_bias * 0.06,
                                       0.34 + height_bias * 0.12,
                                       view_slope);
    float horizon_band = 1.0 - smoothstep(0.03, 0.26, abs(view_slope));
    float vertical_fade = saturate(lerp(lower_air, max(lower_air,
                                                      horizon_band * 0.75),
                                        height_bias));

    float2 drift = float2(uParams1.x * 0.040, -uParams1.x * 0.026);
    float broad_noise = ValueNoise(input.vWorldPos.xz * 0.00125 + drift);
    float pocket_noise =
        ValueNoise(input.vWorldPos.xz * 0.0038 -
                   float2(uParams1.x * 0.090, uParams1.x * 0.055));
    float fine_noise =
        ValueNoise(input.vWorldPos.xz * 0.012 +
                   float2(uParams1.x * 0.14, -uParams1.x * 0.10));

    float pockets = lerp(0.58, 1.22, broad_noise);
    pockets *= lerp(0.72, 1.18, smoothstep(0.18, 0.92, pocket_noise));
    pockets *= lerp(0.94, 1.05, fine_noise);

    float density_alpha =
        (0.010 + density * 0.020) *
        lerp(0.78, 1.20, height_bias);
    float alpha = strength * density_alpha *
                  distance_fade * vertical_fade * pockets;
    alpha = min(saturate(alpha), 0.065 * strength);
    if (alpha <= 0.0006)
    {
        discard;
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    float3 tint = AtmosphericFogTint();
    tint = ApplyBlackWhiteLightOnly(tint);

    return float4(saturate(tint), alpha);
}
