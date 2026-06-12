#version 330 core

in vec4 vShadowClip;

uniform sampler2D uShadowTex;
uniform float uShadowStrength;
uniform int uShadowDebugView;

out vec4 FragColor;

void main()
{
    if (vShadowClip.w <= 0.0)
    {
        if (uShadowDebugView != 0)
        {
            FragColor = vec4(0.0, 0.0, 0.45, 1.0);
            return;
        }

        discard;
    }

    vec3 sndc = vShadowClip.xyz / vShadowClip.w;
    vec2 suv = sndc.xy * 0.5 + 0.5;

    if (suv.x < 0.0 || suv.x > 1.0 ||
        suv.y < 0.0 || suv.y > 1.0 ||
        sndc.z < -1.0 || sndc.z > 1.0)
    {
        if (uShadowDebugView != 0)
        {
            FragColor = vec4(0.0, 0.0, 0.45, 1.0);
            return;
        }

        discard;
    }

    float occluded = texture(uShadowTex, suv).r;
    if (uShadowDebugView != 0)
    {
        FragColor = vec4(occluded, 0.35 + 0.65 * (1.0 - occluded), 0.0, 1.0);
        return;
    }

    float alpha = occluded * uShadowStrength;
    if (alpha <= 0.001)
    {
        discard;
    }

    FragColor = vec4(0.0, 0.0, 0.0, alpha);
}
