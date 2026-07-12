#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float2 vDstUV : TEXCOORD1;
    float4 uColor : TEXCOORD2;
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

float SampleDepth(float2 uv)
{
    return uAuxTexture.Sample(uAuxTextureSampler, saturate(uv)).r;
}

float FogDepth01(float depth)
{
    return smoothstep(0.42, 0.995, depth);
}

float FogContinuity(float2 uv, float depth, float2 texel)
{
    float d_l = SampleDepth(uv + float2(-3.0, 0.0) * texel);
    float d_r = SampleDepth(uv + float2( 3.0, 0.0) * texel);
    float d_u = SampleDepth(uv + float2( 0.0,-3.0) * texel);
    float d_d = SampleDepth(uv + float2( 0.0, 3.0) * texel);

    float nearest = min(min(d_l, d_r), min(d_u, d_d));
    float farthest = max(max(d_l, d_r), max(d_u, d_d));
    float depth_edge = smoothstep(0.025, 0.18, farthest - nearest);
    float foreground_cutout = smoothstep(0.012, 0.09, depth - nearest);

    return 1.0 - saturate(depth_edge * 0.35 + foreground_cutout * 0.55);
}

float FogPocket(float2 uv, float depth, float2 texel)
{
    float d_h0 = SampleDepth(uv + float2(-8.0,  2.0) * texel);
    float d_h1 = SampleDepth(uv + float2( 8.0,  2.0) * texel);
    float d_v0 = SampleDepth(uv + float2( 0.0, -8.0) * texel);
    float d_v1 = SampleDepth(uv + float2( 0.0, 10.0) * texel);

    float opening = 0.0;
    opening += smoothstep(0.025, 0.20, d_h0 - depth);
    opening += smoothstep(0.025, 0.20, d_h1 - depth);
    opening += smoothstep(0.025, 0.20, d_v0 - depth);
    opening += smoothstep(0.025, 0.20, d_v1 - depth);

    return opening * 0.25;
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
    float4 source = uTexture.Sample(uTextureSampler, input.vUV) * input.uColor;

    if (uAtmosphericFogParams.w <= 0.5 || uAtmosphericFogParams.x <= 0.001)
    {
        return source;
    }

    float depth = SampleDepth(input.vUV);
    if (depth >= 0.999)
    {
        return source;
    }

    float2 texture_size = max(uTextureSize.xy, float2(1.0, 1.0));
    float2 texel = 1.0 / texture_size;

    float strength = saturate(uAtmosphericFogParams.x);
    float density = max(uAtmosphericFogParams.y, 0.0);
    float height_bias = saturate(uAtmosphericFogParams.z);

    float depth_fog = FogDepth01(depth);
    float distance_fog =
        (1.0 - exp2(-depth_fog * depth_fog * (density * 2.0 + 0.08))) *
        0.34;

    float lower_screen = smoothstep(0.56, 1.0, input.vUV.y);
    float floor_fog = lower_screen * depth_fog *
                      (0.14 + height_bias * 0.52);
    float horizon_haze =
        (1.0 - smoothstep(0.0, 0.20, abs(input.vUV.y - 0.50))) *
        smoothstep(0.55, 0.99, depth) *
        (0.08 + height_bias * 0.18);

    float pocket_fog = FogPocket(input.vUV, depth, texel) *
                       depth_fog * (0.08 + density * 0.08);

    float continuity = FogContinuity(input.vUV, depth, texel);
    float upper_fade = lerp(0.28, 1.0, smoothstep(0.18, 0.92, input.vUV.y));
    float fog_amount =
        (distance_fog + floor_fog + horizon_haze + pocket_fog) *
        continuity * upper_fade;

    float broad_noise =
        ValueNoise(input.vUV * float2(8.0, 4.5) +
                   float2(uParams1.x * 0.045, -uParams1.x * 0.028));
    float fine_noise =
        ValueNoise(input.vUV * float2(28.0, 16.0) +
                   float2(-uParams1.x * 0.11, uParams1.x * 0.08));
    fog_amount *= lerp(0.82, 1.14, broad_noise) *
                  lerp(0.95, 1.05, fine_noise);

    fog_amount = min(saturate(fog_amount * strength), 0.42 * strength);

    float luma = dot(source.rgb, float3(0.2126, 0.7152, 0.0722));
    float3 tint = AtmosphericFogTint();
    float shadow_lift = fog_amount * (0.18 + 0.42 * (1.0 - luma));
    float3 airlight = source.rgb + tint * shadow_lift;
    float3 color = lerp(airlight, tint, fog_amount * 0.14);

    return float4(saturate(color), source.a);
}
