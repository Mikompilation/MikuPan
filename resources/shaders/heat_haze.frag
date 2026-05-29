#version 330 core

in vec2 vUV;
in vec2 vDstUV;
in vec4 uColor;

out vec4 FragColor;

uniform sampler2D uTexture;

void main()
{
    float mask = clamp(uColor.a, 0.0, 1.0);

    if (mask <= (1.0 / 255.0))
    {
        discard;
    }

    vec2 src_uv = clamp(vUV, vec2(0.0), vec2(1.0));
    vec2 dst_uv = clamp(vDstUV, vec2(0.0), vec2(1.0));

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

    FragColor = vec4(refracted, mask);
}
