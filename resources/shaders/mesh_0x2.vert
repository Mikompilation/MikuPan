#version 330 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vUV;
out vec4 vNormal;

void main()
{
    vUV = aUV;

    gl_Position = projection * view * model * aPos;
    //vNormal = transpose(inverse(model)) * aNormal;
    vNormal = aNormal;
}