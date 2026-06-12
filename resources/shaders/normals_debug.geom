#version 330 core

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in vec4 vNormal[];
in vec4 oViewPosition[];

uniform mat4 projection;
uniform float uNormalLength;

out vec3 gColor;

void main()
{
    for (int i = 0; i < 3; i++)
    {
        vec3 normalVS = normalize(vNormal[i].xyz);
        vec4 startVS = oViewPosition[i];
        vec4 endVS = startVS + vec4(normalVS * uNormalLength, 0.0);

        gl_Position = projection * startVS;
        gColor = vec3(0.0, 1.0, 0.0);
        EmitVertex();

        gl_Position = projection * endVS;
        gColor = vec3(1.0, 0.0, 0.0);
        EmitVertex();

        EndPrimitive();
    }
}
