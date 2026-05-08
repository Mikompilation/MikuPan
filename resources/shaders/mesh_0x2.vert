#version 330 core
layout (location = 0) in vec4 aPos;
layout (location = 1) in vec4 aNormal;
layout (location = 2) in vec2 aUV;

// Pre-computed on the CPU once per object — see mesh_0x12.vert.
uniform mat4 mvp;          // projection * view * model
uniform mat4 modelView;    // view * model
uniform mat4 model;        // world transform — used for shadow projection
uniform mat3 normalMatrix; // transpose(inverse(view * model))

out vec2 vUV;
out vec4 vNormal;
out vec4 oViewPosition;
out vec4 oWorldPosition; // for shadow-space sampling in the fragment shader
out vec3 oVertexColor;

void main()
{
    vUV = aUV;

    gl_Position = mvp * aPos;

    vec3 normalVS = normalize(normalMatrix * vec3(aNormal));
    vNormal = vec4(normalVS, 1.0f);

    oViewPosition  = modelView * aPos;
    oWorldPosition = model * aPos;
    // The 0x2 pipeline has no per-vertex colour attribute (POSITION4_NORMAL4_UV2
    // carries pos/norm/uv only). The PS2 path runs through sgsuv's DRAWTYPE2
    // which calls CalcSpotPoint per-vertex (sgsuv.vsm:153) — i.e. 0x2 is NOT
    // pre-baked by SgPreRender (sglight.c::SetPreRenderMeshData only handles
    // 0x10/0x12/0x32). The shader needs to build the full shade from zero
    // base, so we feed black and the frag's ambient + parallel + point + spot
    // accumulation (uMeshLightingMode=0) produces the lit colour.
    //
    // Using vec3(1) here makes `clamp(white + lights, 0, 1)` saturate to white
    // and the lighting becomes invisible — that's the bug this replaces.
    oVertexColor = vec3(0.0f);
}
