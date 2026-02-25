#version 330 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int isWeighted;

out vec2 vUV;
out vec4 vNormal;

void main()
{
    vUV = aUV;

    if (isWeighted == 1)
    {
        /// Weighted mesh get converted to world space on the CPU
        gl_Position = projection * view * aPos;
        vNormal = aNormal;
    }
    else
    {
        gl_Position = projection * view * model * aPos;
        vNormal = model * aNormal;
    }

}