#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vUV;
out vec4 vNormal;
out vec4 oViewPosition;
out vec3 oVertexColor;

void main()
{
    vUV = aUV;
    gl_Position = projection * view * model * vec4(aPos, 1.0f);

    mat3 normalMat = mat3(transpose(inverse(view * model)));
    vec3 normalVS = normalize(normalMat * aNormal);
    vNormal = vec4(normalVS, 1.0f);

    vec4 a = view * model * vec4(aPos, 1.0f);
    oViewPosition = a;
    oVertexColor = vec3(aColor.r / 255.0f, aColor.g / 255.0f, aColor.b / 255.0f);

    if (oVertexColor.r == 0.0f && oVertexColor.g == 0.0f && oVertexColor.b == 0.0f)
    {
        oVertexColor = vec3(0.0f, 0.0f, 0.0f);
    }
}