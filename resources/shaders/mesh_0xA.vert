#version 330 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vUV;
out vec4 vNormal;
out vec4 oViewPosition;

void main()
{
    vUV = aUV;

    /// Weighted mesh get converted to world space on the CPU
    gl_Position = projection * view * aPos;

    mat3 normalMat = mat3(transpose(inverse(view)));
    vec3 normalVS = normalize(normalMat * vec3(aNormal));
    vNormal = vec4(normalVS, 1.0f);

    vec4 a = view * aPos;
    oViewPosition = a;

}