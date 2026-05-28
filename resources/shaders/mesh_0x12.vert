#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4 mvp;
uniform mat4 modelView;
uniform mat3 normalMatrix;

layout(std140) uniform LightBlock
{
    vec4 uAmbient;

    ivec4 uParCount;
    vec4 uParDir[3];
    vec4 uParDiffuse[3];
    vec4 uParSpecular[3];
    vec4 uParHalfway[3];

    ivec4 uPointCount;
    vec4 uPointPos[3];
    vec4 uPointDiffuse[3];
    vec4 uPointSpecular[3];
    vec4 uPointPower[3];

    ivec4 uSpotCount;
    vec4 uSpotPos[3];
    vec4 uSpotDir[3];
    vec4 uSpotDiffuse[3];
    vec4 uSpotSpecular[3];
    vec4 uSpotPower[3];
    vec4 uSpotIntens[3];
    vec4 uMaterialAlpha;
};

uniform int uMeshLightingMode;

out vec2 vUV;
out vec4 vNormal;
out vec4 oViewPosition;
out vec4 oWorldPosition; // for shadow-space sampling in the fragment shader
out vec3 oVertexColor;
noperspective out vec3 oLitVertexColor;

vec3 CalcPS2LitColor(vec4 normal, vec4 viewPos, vec3 vertexColor)
{
    vec3 N = normalize(normal.xyz);
    const float parallel_shininess = 4.0;
    vec3 vc = vertexColor + uAmbient.rgb;

    for (int i = 0; i < uParCount.x; i++)
    {
        float NdotL = max(dot(N, uParDir[i].xyz), 0.0);
        float NdotH = max(dot(N, uParHalfway[i].xyz), 0.0);
        vc += uParDiffuse[i].rgb * NdotL;
        vc += uParSpecular[i].rgb * pow(NdotH, parallel_shininess);
    }

    for (int i = 0; i < uPointCount.x; i++)
    {
        vec3 L = uPointPos[i].xyz - viewPos.xyz;
        float dist2 = dot(L, L);
        if (dist2 <= 0.0) continue;

        float inv_dist = inversesqrt(dist2);
        vec3 Ldir = L * inv_dist;
        float NdotL = max(dot(N, Ldir), 0.0);
        float intensity = min(NdotL * uPointPower[i].x * inv_dist, 1.0);

        vc += uPointDiffuse[i].rgb * intensity;

        float spec = intensity * intensity;
        spec *= spec;
        spec *= spec;
        vc += uPointSpecular[i].rgb * spec;
    }

    for (int i = 0; i < uSpotCount.x; i++)
    {
        vec3 L = uSpotPos[i].xyz - viewPos.xyz;
        float dist2 = dot(L, L);
        if (dist2 <= 0.0) continue;

        float inv_dist = inversesqrt(dist2);
        vec3 Ldir = L * inv_dist;
        vec3 Z = normalize(uSpotDir[i].xyz);
        float cd = max(dot(Ldir, Z), 0.0);
        float cos2 = cd * cd;
        float gate = max(cos2 - uSpotIntens[i].x, 0.0) * uSpotIntens[i].y;
        gate = clamp(gate, 0.0, 1.0);
        if (cos2 <= uSpotIntens[i].x) continue;

        float NdotL = max(dot(N, Ldir), 0.0);
        float intensity = min(NdotL * uSpotPower[i].x * inv_dist, 1.0);

        vc += uSpotDiffuse[i].rgb * intensity * gate;

        float spec = intensity * intensity;
        spec *= spec;
        spec *= spec;
        vc += uSpotSpecular[i].rgb * spec * gate;
    }

    return vc;
}

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0f);
    vUV = aUV;

    //mat3 normalMat = mat3(transpose(inverse(view * model)));
    //vec3 normalVS = normalize(normalMat * aNormal);
    //vNormal = vec4(normalVS, 1.0f);

    vec3 normalVS = normalize(normalMatrix * aNormal);
    vec4 normal = vec4(normalVS, 1.0f);
    vNormal = normal;

    vec4 viewPosition = modelView * vec4(aPos, 1.0f);
    oViewPosition = viewPosition;
    oWorldPosition = model * vec4(aPos, 1.0f);

    vec3 vertexColor = aColor;
    oVertexColor = vertexColor;
    oLitVertexColor = uMeshLightingMode == 1
        ? CalcPS2LitColor(normal, viewPosition, vertexColor)
        : vec3(0.0);
}
