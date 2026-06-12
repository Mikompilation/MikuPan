#version 330 core
layout(location = 0) in vec4 aColour;
layout(location = 1) in vec4 aPos;

out vec4 outColor;

void main()
{
    gl_Position = aPos;
    outColor = aColour;
}