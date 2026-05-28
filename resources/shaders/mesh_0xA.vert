#version 330 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4 viewProj;
uniform mat3 viewNormalMatrix;

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
out vec4 oWorldPosition;
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
    vUV = aUV;

    gl_Position = projection * view * aPos;

    //mat3 normalMat = mat3(transpose(inverse(view)));
    //vec3 normalVS = normalize(normalMat * vec3(aNormal));
    //vNormal = vec4(normalVS, 1.0f);

    vec3 normalVS = normalize(mat3(view) * vec3(aNormal));
    vec4 normal = vec4(normalVS, 1.0f);
    vNormal = normal;

    vec4 viewPosition = view * aPos;
    oViewPosition = viewPosition;
    oWorldPosition = aPos;
    vec3 vertexColor = vec3(0.0f);
    oVertexColor = vertexColor;
    oLitVertexColor = uMeshLightingMode == 1
        ? CalcPS2LitColor(normal, viewPosition, vertexColor)
        : vec3(0.0);

    /*
    vUV = aUV;

    gl_Position = viewProj * aPos;

    vec3 normalVS = normalize(viewNormalMatrix * vec3(aNormal));
    vNormal = vec4(normalVS, 1.0f);

    oViewPosition = view * aPos;
    // 0xA is already in world space (CPU-skinned); position passes through
    // unchanged for the shadow projection.
    oWorldPosition = aPos;
    // 0xA shares the no-colour pipeline with 0x2. Same VU dispatch (sgsuv
    // DRAWTYPE2 → CalcSpotPoint), no SgPreRender pre-bake. Use a zero base so
    // the frag's full dynamic lighting (uMeshLightingMode=0) shows through;
    // see mesh_0x2.vert for the full rationale.
    oVertexColor = vec3(0.0f);
    */
}
