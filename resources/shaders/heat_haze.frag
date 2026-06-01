#version 330 core

in vec4 vUVData;
in vec4 uColor;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2 uFramebufferUvOffset;
uniform vec2 uFramebufferUvScale;
uniform vec2 uFramebufferContentUvMax;
uniform int uUseScreenPos;
uniform vec2 uRenderSize;

void main()
{
    float mask = clamp(uColor.a, 0.0, 1.0);

    if (mask <= (1.0 / 255.0))
    {
        discard;
    }

    vec2 src_uv;
    vec2 dst_uv;
    bool resolve_deform = false;

    if (uUseScreenPos == 1)
    {
        // gl_FragCoord origin is bottom-left; the screen copy blit flips Y, so invert Y.
        // The render buffer has the full 3D scene across its entire extent (no letterbox),
        // so clamp to [0,1] — not to the PS2 content region.
        vec2 screen_uv = clamp(vec2(gl_FragCoord.x / uRenderSize.x,
                                    1.0 - gl_FragCoord.y / uRenderSize.y),
                               vec2(0.0), vec2(1.0));
        src_uv = screen_uv;
        dst_uv = screen_uv;
    }
    else if (uUseScreenPos == 2)
    {
        vec2 screen_uv = clamp(vec2(gl_FragCoord.x / uRenderSize.x,
                                    1.0 - gl_FragCoord.y / uRenderSize.y),
                               vec2(0.0), vec2(1.0));
        src_uv = clamp(vUVData.xy, vec2(0.0), vec2(1.0));
        dst_uv = screen_uv;
        resolve_deform = true;
    }
    else
    {
        src_uv = clamp(
            uFramebufferUvOffset + vUVData.xy * uFramebufferUvScale,
            uFramebufferUvOffset,
            uFramebufferContentUvMax);
        dst_uv = clamp(
            uFramebufferUvOffset + vUVData.zw * uFramebufferUvScale,
            uFramebufferUvOffset,
            uFramebufferContentUvMax);
    }

    vec3 src = texture(uTexture, src_uv).rgb;
    vec3 dst = texture(uTexture, dst_uv).rgb;

    /*
     * The PS2 source texture is a local framebuffer copy with TCC=RGB. Its
     * alpha channel is not the mask for this effect; the particle vertex alpha
     * is. RGB controls how much of the shifted copy is used.
     */
    float color_strength = max(max(uColor.r, uColor.g), uColor.b);
    float refract_strength = clamp(color_strength, 0.0, 1.0);
    vec3 refracted = mix(dst, src, refract_strength);

    if (resolve_deform)
    {
        FragColor = vec4(mix(dst, refracted, mask), 1.0);
        return;
    }

    FragColor = vec4(refracted, mask);
}
