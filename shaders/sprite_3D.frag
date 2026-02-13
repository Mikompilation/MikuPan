#version 330 core

in vec2 vUV;
in vec4 uColor;
out vec4 FragColor;

uniform sampler2D uTex;

//uniform float uAlphaRef;   // from GS TEST register
//uniform int uAlphaEnable;  // emulate ATE

void main()
{
    vec4 tex = texture(uTex, vUV);
    vec4 col = tex * uColor;

    // PS2 alpha test emulation
    //if (uAlphaEnable == 1)
    //{
    //    if (col.a < uAlphaRef)
    //    discard;
    //}

    FragColor = col;
}