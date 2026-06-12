#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 mvp;
uniform mat4 uShadowMatrix;

out vec4 vShadowClip;

void main()
{
    vec4 localPos = vec4(aPos, 1.0);
    vec4 worldPos = model * localPos;

    gl_Position = mvp * localPos;
    vShadowClip = uShadowMatrix * worldPos;
}
