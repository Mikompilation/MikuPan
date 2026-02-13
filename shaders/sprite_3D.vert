#version 330 core

layout(location = 0) in vec2 aUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

//uniform vec2 uHalfPixel;   // (0.5/width, 0.5/height)
//uniform float uDepthBias;

out vec2 vUV;
out vec4 uColor;

void main()
{
    // PS2 half-pixel correction
    //pos.xy += pos.w * vec2(uHalfPixel.x, -uHalfPixel.y);

    // PS2 depth bias (XYZF2 style)
    //pos.z += uDepthBias * pos.w;

    gl_Position = aPos;
    vUV = aUV;
    uColor = inColor;
}