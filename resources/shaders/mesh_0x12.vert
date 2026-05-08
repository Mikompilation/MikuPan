#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUV;
layout (location = 3) in vec3 aColor;

// Pre-computed on the CPU once per object so we don't pay an inverse() and
// two extra mat4*mat4 multiplies on every vertex (which most drivers fail
// to hoist out of the per-vertex code).
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
    gl_Position = mvp * vec4(aPos, 1.0f);

    vec3 normalVS = normalize(normalMatrix * aNormal);
    vNormal = vec4(normalVS, 1.0f);

    oViewPosition  = modelView * vec4(aPos, 1.0f);
    oWorldPosition = model * vec4(aPos, 1.0f);

    // For 0x10/0x12/0x32 the per-vertex colour is the SgPreRender output —
    // ambient+point+spot already baked, so we keep the raw value and let the
    // frag shader add only parallel + ambient on top (uMeshLightingMode=1).
    //
    // For 0x82 the renderer fills the colour buffer with zeros (NVL meshes
    // have no per-vertex light input on the PS2). Letting that zero pass
    // through means the frag shader builds the entire shade from
    // ambient + parallel + point + spot (uMeshLightingMode=0). A previous
    // build promoted (0,0,0) to (1,1,1) here as a band-aid for 0x82 going
    // pitch black — that masked the lighting calculation. The frag's
    // ambient_term + dynamic-light accumulation now produces the right
    // brightness, so the fallback is gone.
    oVertexColor = aColor;
}
