#version 330 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec2 aUV;

// 0xA vertices are CPU-skinned into world space — no per-object model matrix.
// All matrix derivations are pre-computed once per frame on the CPU.
uniform mat4 viewProj;         // projection * view
uniform mat4 view;             // view (for view-space position)
uniform mat3 viewNormalMatrix; // transpose(inverse(view))

out vec2 vUV;
out vec4 vNormal;
out vec4 oViewPosition;
out vec4 oWorldPosition; // for shadow-space sampling in the fragment shader
out vec3 oVertexColor;

void main()
{
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
}
