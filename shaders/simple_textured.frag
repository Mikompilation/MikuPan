#version 330 core
in vec2 vUV;
in vec3 vNormal;

out vec4 FragColor;

uniform sampler2D uTexture;
uniform int renderNormals;

void main()
{
    if (renderNormals == 1)
    {
        FragColor = vec4(normalize(vNormal), 1.0f);
    }
    else
    {
        vec3 lightDir = normalize(vec3(0.3, 0.7, 0.6));
        float ndl = max(dot(normalize(vNormal), -lightDir), 0.0);
        vec4 tex = texture(uTexture, vUV);
        FragColor = vec4(tex.rgb, tex.a);

        //if (tex.a == 1.0f)
        //{
        //    discard;
        //}
    }
}