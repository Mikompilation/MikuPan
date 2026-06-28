#include "mikupan_common.hlsli"

struct PSInput
{
    float4 position : SV_Position;
    float2 vUV : TEXCOORD0;
    float2 vDstUV : TEXCOORD1;
    float4 uColor : TEXCOORD2;
};

static const float TAU = 6.28318530718;

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453123);
}

float3 ToneMap(float3 color)
{
    color *= uParams0.z;
    return pow(max(color, 0.0.xxx), (1.0 / max(uParams0.w, 0.01)).xxx);
}

float2 ClampTexelUv(float2 uv)
{
    float2 texel = 1.0 / max(uTextureSize.xy, 1.0.xx);
    return clamp(uv, texel * 0.5, 1.0.xx - texel * 0.5);
}

float3 FetchSceneCrt(float2 uv)
{
    float2 texel = 1.0 / max(uTextureSize.xy, 1.0.xx);
    float shift = uCrt2.z * texel.x;

    float3 color;
    color.r = uTexture.Sample(uTextureSampler, ClampTexelUv(uv + float2( shift, 0.0))).r;
    color.g = uTexture.Sample(uTextureSampler, ClampTexelUv(uv)).g;
    color.b = uTexture.Sample(uTextureSampler, ClampTexelUv(uv + float2(-shift, 0.0))).b;

    return color * uColor.rgb;
}

float3 BlendSceneCrt(float2 uv)
{
    float3 center = FetchSceneCrt(uv);
    float strength = clamp(uCrt2.w, 0.0, 1.0);
    if (strength <= 0.001)
    {
        return center;
    }

    float2 texel = 1.0 / max(uTextureSize.xy, 1.0.xx);
    float2 radius = texel * max(uCrt3.x, 0.0);

    float3 blended = center * 0.40;
    blended += FetchSceneCrt(uv + float2( radius.x, 0.0)) * 0.20;
    blended += FetchSceneCrt(uv + float2(-radius.x, 0.0)) * 0.20;
    blended += FetchSceneCrt(uv + float2(0.0,  radius.y)) * 0.10;
    blended += FetchSceneCrt(uv + float2(0.0, -radius.y)) * 0.10;

    return lerp(center, blended, strength);
}

float2 WarpCrtUv(float2 uv)
{
    float2 centered = uv * 2.0 - 1.0;
    float radius2 = dot(centered, centered);

    centered *= 1.0 + uCrt0.y * radius2;
    centered *= 1.0 + uCrt0.z;

    return centered * 0.5 + 0.5;
}

float3 ApplyCrt(float2 uv, float2 fragCoord)
{
    float2 warped = WarpCrtUv(uv);
    if (warped.x < 0.0 || warped.y < 0.0 || warped.x > 1.0 || warped.y > 1.0)
    {
        return 0.0.xxx;
    }

    float2 texel = 1.0 / max(uTextureSize.xy, 1.0.xx);
    float3 color = BlendSceneCrt(warped);

    float3 glow = FetchSceneCrt(warped + float2( texel.x, 0.0));
    glow += FetchSceneCrt(warped + float2(-texel.x, 0.0));
    glow += FetchSceneCrt(warped + float2(0.0,  texel.y));
    glow += FetchSceneCrt(warped + float2(0.0, -texel.y));
    glow *= 0.25;
    color = lerp(color, glow, clamp(uCrt3.w, 0.0, 1.0));

    float scan_pos = warped.y * max(uTextureSize.y, 1.0) * max(uCrt1.x, 0.01);
    float scan_wave = 0.5 + 0.5 * cos(scan_pos * TAU);
    float scanline = pow(scan_wave, max(uCrt1.y, 0.01));
    color *= 1.0 - clamp(uCrt0.w, 0.0, 1.0) * scanline;

    float2 output_size = max(uOutputSize.xy, 1.0.xx);
    float pixel_scale = max(output_size.x / max(uTextureSize.x, 1.0), 1.0);
    float triad = fmod((fragCoord.x / pixel_scale) * max(uCrt1.w, 0.01), 3.0);
    float3 mask = triad < 1.0 ? float3(1.00, 0.72, 0.72) :
                  triad < 2.0 ? float3(0.72, 1.00, 0.72) :
                                float3(0.72, 0.72, 1.00);
    color *= lerp(1.0.xxx, mask, clamp(uCrt1.z, 0.0, 1.0));

    float2 vignette_uv = warped * 2.0 - 1.0;
    float vignette = smoothstep(clamp(uCrt2.y, 0.01, 2.0),
                                1.35, length(vignette_uv));
    color *= 1.0 - vignette * clamp(uCrt2.x, 0.0, 1.0);

    float noise = Hash(floor(fragCoord) + float2(uParams1.x * 47.0, uParams1.x * 13.0)) - 0.5;
    color += noise * uCrt3.y;

    float flicker = 1.0 + sin(uParams1.x * 75.0) * uCrt3.z;
    color *= flicker;

    return color;
}


float3 AdjustSaturationPs2(float3 color, float saturation)
{
    float luma = dot(color, float3(0.299, 0.587, 0.114));
    return lerp(luma.xxx, color, saturation);
}

float3 ApplyPs2FeedbackGrade(float3 color, float2 uv)
{
    if (uPadFlags.x == 0)
    {
        return color;
    }

    float strength = clamp(uPs2Feedback.x, 0.0, 1.0);
    if (strength <= 0.001)
    {
        return color;
    }

    if (uPadFlags.y != 0)
    {
        float ghost = clamp(uPs2Feedback.w * strength, 0.0, 0.20);
        if (ghost > 0.0001)
        {
            float3 previous = uAuxTexture.Sample(uAuxTextureSampler, ClampTexelUv(uv)).rgb;
            color = lerp(color, previous, ghost);
        }
    }

    float burn = clamp(uPs2Feedback.y * strength, 0.0, 1.0);
    color = lerp(color, color * color, burn);

    float saturation = lerp(1.0, clamp(uPs2Feedback.z, 0.5, 1.5), strength);
    color = AdjustSaturationPs2(color, saturation);

    return color;
}

float4 main(PSInput input) : SV_Target0
{
    float4 source = uTexture.Sample(uTextureSampler, input.vUV) * input.uColor;
    float3 color = source.rgb;

    color = ApplyPs2FeedbackGrade(color, input.vUV);

    if (uFlags1.w != 0 && uCrt0.x > 0.001)
    {
        color = lerp(color, ApplyCrt(input.vUV, input.position.xy), clamp(uCrt0.x, 0.0, 1.0));
    }

    if (uFlags2.x != 0)
    {
        color = ToBlackWhite(color);
    }

    if (uFlags2.y != 0)
    {
        bool in_content =
            input.vUV.x >= uPhotoNegativeContentRect.x &&
            input.vUV.y >= uPhotoNegativeContentRect.y &&
            input.vUV.x <= uPhotoNegativeContentRect.z &&
            input.vUV.y <= uPhotoNegativeContentRect.w;
        bool in_photo =
            input.vUV.x >= uPhotoNegativeRect.x &&
            input.vUV.y >= uPhotoNegativeRect.y &&
            input.vUV.x <= uPhotoNegativeRect.z &&
            input.vUV.y <= uPhotoNegativeRect.w;

        if (in_content && !in_photo)
        {
            float strength = clamp(uParams1.y, 0.0, 1.0);
            float3 negative_source = color;
            if (uFlags2.z != 0)
            {
                float2 source_uv =
                    (input.vUV - uPhotoNegativeContentRect.xy) /
                    max(uPhotoNegativeContentRect.zw -
                        uPhotoNegativeContentRect.xy,
                        0.0001.xx);
                source_uv = clamp(source_uv, 0.0.xx, 1.0.xx);
                negative_source =
                    uAuxTexture.Sample(uAuxTextureSampler, source_uv).rgb *
                    input.uColor.rgb;
            }

            float3 negative_color =
                clamp((96.0 / 255.0).xxx - negative_source, 0.0.xxx, 1.0.xxx);
            color = lerp(color, negative_color, strength);
        }
    }

    if (uScreenNegative.a > 0.001)
    {
        float strength = clamp(uScreenNegative.a, 0.0, 1.0);
        float3 negative_color =
            clamp(uScreenNegative.rgb - color, 0.0.xxx, 1.0.xxx);
        color = lerp(color, negative_color, strength);
    }

    return float4(clamp(ToneMap(color), 0.0.xxx, 1.0.xxx), source.a);
}
