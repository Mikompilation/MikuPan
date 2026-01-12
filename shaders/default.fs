#version 330 core
out vec4 FragColor;
in vec4 vertexColor;
void main()
{
   FragColor = vec4(normalize(vec3(vertexColor)), 1.0f);
}