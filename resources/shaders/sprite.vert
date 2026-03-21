#version 330 core

layout(location = 0) in vec4 aUV;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec4 aPos;

out vec2 vUV;
out vec4 uColor;

void main()
{
    gl_Position = aPos;
    vUV = vec2(aUV);
    uColor = inColor;
}