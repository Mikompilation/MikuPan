#version 330 core

in vec2 vUV;
in vec4 uColor;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uPhotoNegativeStrength;

void main()
{
    vec4 tex = texture(uTexture, vUV);
    vec4 col = tex * uColor;

    if (uPhotoNegativeStrength > 0.001)
    {
        float strength = clamp(uPhotoNegativeStrength, 0.0, 1.0);
        vec3 negative_color =
            clamp(vec3(96.0 / 255.0) - col.rgb, 0.0, 1.0);
        col.rgb = mix(col.rgb, negative_color, strength);
    }

    //if (col.a == 0.0f)
    //{
    //    discard;
    //}

    FragColor = col;
}
