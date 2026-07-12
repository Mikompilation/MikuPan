#version 330 core

in vec4 vUVData;
in vec4 uColor;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2 uRenderSize;

vec3 SampleScene(vec2 uv)
{
    return texture(uTexture, clamp(uv, vec2(0.0), vec2(1.0))).rgb;
}

void main()
{
    float fade = clamp(uColor.a, 0.0, 1.0);
    if (fade <= (1.0 / 255.0))
    {
        discard;
    }

    vec2 uv = clamp(vec2(gl_FragCoord.x / max(uRenderSize.x, 1.0),
                         1.0 - gl_FragCoord.y / max(uRenderSize.y, 1.0)),
                    vec2(0.0), vec2(1.0));
    uv = clamp(uv + vUVData.xy * 0.000001, vec2(0.0), vec2(1.0));
    vec2 texel = 1.0 / max(uRenderSize, vec2(1.0));

    vec3 blur = SampleScene(uv) * 0.080;
    blur += SampleScene(uv + texel * vec2( 14.0,   0.0)) * 0.060;
    blur += SampleScene(uv + texel * vec2(-14.0,   0.0)) * 0.060;
    blur += SampleScene(uv + texel * vec2(  0.0,  14.0)) * 0.060;
    blur += SampleScene(uv + texel * vec2(  0.0, -14.0)) * 0.060;
    blur += SampleScene(uv + texel * vec2( 25.0,  15.0)) * 0.050;
    blur += SampleScene(uv + texel * vec2(-25.0, -15.0)) * 0.050;
    blur += SampleScene(uv + texel * vec2(-25.0,  15.0)) * 0.050;
    blur += SampleScene(uv + texel * vec2( 25.0, -15.0)) * 0.050;
    blur += SampleScene(uv + texel * vec2( 43.0,   0.0)) * 0.042;
    blur += SampleScene(uv + texel * vec2(-43.0,   0.0)) * 0.042;
    blur += SampleScene(uv + texel * vec2(  0.0,  43.0)) * 0.042;
    blur += SampleScene(uv + texel * vec2(  0.0, -43.0)) * 0.042;
    blur += SampleScene(uv + texel * vec2( 42.0,  35.0)) * 0.035;
    blur += SampleScene(uv + texel * vec2(-42.0, -35.0)) * 0.035;
    blur += SampleScene(uv + texel * vec2(-42.0,  35.0)) * 0.035;
    blur += SampleScene(uv + texel * vec2( 42.0, -35.0)) * 0.035;
    blur += SampleScene(uv + texel * vec2( 66.0,  20.0)) * 0.027;
    blur += SampleScene(uv + texel * vec2(-66.0, -20.0)) * 0.027;
    blur += SampleScene(uv + texel * vec2(-66.0,  20.0)) * 0.027;
    blur += SampleScene(uv + texel * vec2( 66.0, -20.0)) * 0.027;

    float lens_vignette = smoothstep(0.88, 0.22, length((uv - vec2(0.5)) * vec2(1.0, 0.82)));
    vec3 lens_tint = vec3(0.72, 0.67, 0.82);
    vec3 out_rgb = blur * mix(0.42, 0.74, lens_vignette) * lens_tint;

    FragColor = vec4(out_rgb, fade);
}
