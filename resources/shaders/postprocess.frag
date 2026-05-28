#version 330 core

in vec2 vUV;
in vec4 uColor;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uBrightness;
uniform float uGamma;

uniform int uCrtEnabled;
uniform vec2 uTextureSize;
uniform vec2 uOutputSize;
uniform float uTime;
uniform float uCrtStrength;
uniform float uCrtCurvature;
uniform float uCrtOverscan;
uniform float uCrtScanlineStrength;
uniform float uCrtScanlineScale;
uniform float uCrtScanlineThickness;
uniform float uCrtMaskStrength;
uniform float uCrtMaskScale;
uniform float uCrtVignetteStrength;
uniform float uCrtVignetteSize;
uniform float uCrtChromaOffset;
uniform float uCrtBlendStrength;
uniform float uCrtBlendRadius;
uniform float uCrtNoiseStrength;
uniform float uCrtFlickerStrength;
uniform float uCrtGlowStrength;

const float TAU = 6.28318530718;

float Hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

vec3 ToneMap(vec3 color)
{
    color *= uBrightness;
    return pow(max(color, vec3(0.0)), vec3(1.0 / max(uGamma, 0.01)));
}

vec2 ClampTexelUv(vec2 uv)
{
    vec2 texel = 1.0 / max(uTextureSize, vec2(1.0));
    return clamp(uv, texel * 0.5, vec2(1.0) - texel * 0.5);
}

vec3 FetchSceneCrt(vec2 uv)
{
    vec2 texel = 1.0 / max(uTextureSize, vec2(1.0));
    float shift = uCrtChromaOffset * texel.x;

    vec3 color;
    color.r = texture(uTexture, ClampTexelUv(uv + vec2( shift, 0.0))).r;
    color.g = texture(uTexture, ClampTexelUv(uv)).g;
    color.b = texture(uTexture, ClampTexelUv(uv + vec2(-shift, 0.0))).b;

    return color * uColor.rgb;
}

vec3 BlendSceneCrt(vec2 uv)
{
    vec3 center = FetchSceneCrt(uv);
    float strength = clamp(uCrtBlendStrength, 0.0, 1.0);
    if (strength <= 0.001)
    {
        return center;
    }

    vec2 texel = 1.0 / max(uTextureSize, vec2(1.0));
    vec2 radius = texel * max(uCrtBlendRadius, 0.0);

    vec3 blended = center * 0.40;
    blended += FetchSceneCrt(uv + vec2( radius.x, 0.0)) * 0.20;
    blended += FetchSceneCrt(uv + vec2(-radius.x, 0.0)) * 0.20;
    blended += FetchSceneCrt(uv + vec2(0.0,  radius.y)) * 0.10;
    blended += FetchSceneCrt(uv + vec2(0.0, -radius.y)) * 0.10;

    return mix(center, blended, strength);
}

vec2 WarpCrtUv(vec2 uv)
{
    vec2 centered = uv * 2.0 - 1.0;
    float radius2 = dot(centered, centered);

    centered *= 1.0 + uCrtCurvature * radius2;
    centered *= 1.0 + uCrtOverscan;

    return centered * 0.5 + 0.5;
}

vec3 ApplyCrt(vec2 uv)
{
    vec2 warped = WarpCrtUv(uv);
    if (warped.x < 0.0 || warped.y < 0.0 || warped.x > 1.0 || warped.y > 1.0)
    {
        return vec3(0.0);
    }

    vec2 texel = 1.0 / max(uTextureSize, vec2(1.0));
    vec3 color = BlendSceneCrt(warped);

    vec3 glow = FetchSceneCrt(warped + vec2( texel.x, 0.0));
    glow += FetchSceneCrt(warped + vec2(-texel.x, 0.0));
    glow += FetchSceneCrt(warped + vec2(0.0,  texel.y));
    glow += FetchSceneCrt(warped + vec2(0.0, -texel.y));
    glow *= 0.25;
    color = mix(color, glow, clamp(uCrtGlowStrength, 0.0, 1.0));

    float scan_pos = warped.y * max(uTextureSize.y, 1.0) * max(uCrtScanlineScale, 0.01);
    float scan_wave = 0.5 + 0.5 * cos(scan_pos * TAU);
    float scanline = pow(scan_wave, max(uCrtScanlineThickness, 0.01));
    color *= 1.0 - clamp(uCrtScanlineStrength, 0.0, 1.0) * scanline;

    vec2 output_size = max(uOutputSize, vec2(1.0));
    float pixel_scale = max(output_size.x / max(uTextureSize.x, 1.0), 1.0);
    float triad = mod((gl_FragCoord.x / pixel_scale) * max(uCrtMaskScale, 0.01), 3.0);
    vec3 mask = triad < 1.0 ? vec3(1.00, 0.72, 0.72) :
                triad < 2.0 ? vec3(0.72, 1.00, 0.72) :
                              vec3(0.72, 0.72, 1.00);
    color *= mix(vec3(1.0), mask, clamp(uCrtMaskStrength, 0.0, 1.0));

    vec2 vignette_uv = warped * 2.0 - 1.0;
    float vignette = smoothstep(clamp(uCrtVignetteSize, 0.01, 2.0),
                                1.35, length(vignette_uv));
    color *= 1.0 - vignette * clamp(uCrtVignetteStrength, 0.0, 1.0);

    float noise = Hash(floor(gl_FragCoord.xy) + vec2(uTime * 47.0, uTime * 13.0)) - 0.5;
    color += noise * uCrtNoiseStrength;

    float flicker = 1.0 + sin(uTime * 75.0) * uCrtFlickerStrength;
    color *= flicker;

    return color;
}

void main()
{
    vec4 source = texture(uTexture, vUV) * uColor;
    vec3 color = source.rgb;

    if (uCrtEnabled != 0 && uCrtStrength > 0.001)
    {
        color = mix(color, ApplyCrt(vUV), clamp(uCrtStrength, 0.0, 1.0));
    }

    FragColor = vec4(clamp(ToneMap(color), 0.0, 1.0), source.a);
}
