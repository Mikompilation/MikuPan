#version 330 core

layout(std140) uniform LightBlock
{
    vec4 uAmbient;

    ivec4 uParCount;
    vec4 uParDir[3];
    vec4 uParDiffuse[3];
    vec4 uParSpecular[3];   // matches SgLIGHT.specular per sglight.c:116
    vec4 uParHalfway[3];    // pre-normalized (-eye + dir) per sglight.c:118-120

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

/// Texture Uniforms
uniform sampler2D uTexture;
uniform int renderNormals;

/// Shadow uniforms — fed by the renderer when MikuPan_IsShadowEnabled() is on.
/// uShadowMatrix is the shadow camera's world-clip-view (PS2 scamera.wcv,
/// captured inside MikuPan_BeginShadowPass). Sampling outside the shadow
/// frustum is gated by the texture's CLAMP_TO_BORDER + zero border colour
/// so out-of-frustum fragments always read a 0 alpha and do nothing.
uniform sampler2D uShadowTex;
uniform mat4      uShadowMatrix;
uniform int       uShadowEnabled;
uniform float     uShadowStrength;

/// Debug toggle: when set, skip ApplyPS2Lights entirely and output the raw
/// texture × vertex-color albedo. Fog is still applied so depth perception
/// stays intact. Driven by the "Disable Lighting" UI checkbox.
uniform int disableLighting;
uniform int staticLighting;
uniform int uMeshLightingMode; // 0 = per-fragment, 1 = per-vertex

/// Fog uniforms (kept as regular uniforms)
uniform vec4 uFog;      // x=min, y=max, z=base, w=scale
uniform vec4 uFogColor;

/// Input variables from vertex shader
in vec2 vUV;
in vec4 vNormal;
in vec4 oViewPosition;
in vec4 oWorldPosition;
in vec3 oVertexColor;
noperspective in vec3 oLitVertexColor;

out vec4 FragColor;

const float kGsModulateScale = 255.0 / 128.0;

vec3 ApplyGsModulate(vec3 textureColor, vec3 ps2Color255)
{
    // GS TFX=MODULATE uses (texture * vertex_color) >> 7. In normalized GL
    // terms the VU/GS color bus is clamped to 0..255, but 128 is neutral.
    return clamp(clamp(ps2Color255, vec3(0.0), vec3(1.0))
                 * textureColor * kGsModulateScale,
                 vec3(0.0), vec3(1.0));
}

float GetMaterialAlpha()
{
    return clamp(uMaterialAlpha.x, 0.0, 1.0);
}

// PS2 lighting model — matches the VU1 microcode in vu1/sgsuv/sgexlit.vsm
// and the C reference impls in graphics/graph3d/sglight.c (CalcIntens for
// parallel lights, _CalcPointA/B for points, asm_CalcSpotLight for spots).
//
// Per-vertex accumulation into vf18:
//
//   vf18 = Parallel_Ambient                              // ambient + emission
//
//   for each PARALLEL light i:                           // CalcIntens
//       NdotL = max(N · DLightMtx[i], 0)                 //   light dir (toward light)
//       NdotH = max(N · SLightMtx[i], 0)                 //   halfway = norm(ieye + dir)
//       vf18 += DColorMtx[i] × NdotL                     // diffuse
//       vf18 += SColorMtx[i] × NdotH^n                   // Blinn-Phong specular (n≈4)
//
//   for each POINT light i:                              // _CalcPointA/B (sglight.c:1024)
//       L        = lightPos - vert                       //   vertex to light
//       inv_d2   = 1 / |L|^2
//       NdotL_b  = max(rotated_lDir · L, 0) × btimes     // btimes = power×max_color
//       I        = min(NdotL_b × inv_d2, 1)              //   diffuse intensity, clamped
//       vf18 += DColor[i] × I                            // diffuse
//       vf18 += SColor[i] × I^8                          // FAKE-PHONG specular: pow8 of
//                                                        // diffuse intensity, NOT NdotH.
//
//   for each SPOT light i:                               // asm_CalcSpotLight (sglight.c:1079)
//       cosθ²  = (WDLightMtx · L)² × inv_d2              //   cone-axis alignment
//       gate   = max(cosθ² - intens, 0) × intens_b       //   smooth ramp 0..1 across
//                                                        //   the intens..1 window
//       I      = min(NdotL_b × inv_d2, 1)
//       vf18 += DColor[i] × I    × gate
//       vf18 += SColor[i] × I^8  × gate                  //   same fake-Phong as point
//
//   vf18 saturate; GS MODULATE pairs it with the texture at fragment time.
//
// Two structural notes:
//   1. The lighting is ADDITIVE on top of the per-vertex colour, NOT
//      multiplicative. Compute `vc` that mirrors vf18, then do ONE multiply
//      by the texture at the end (the GS modulate equivalent).
//   2. POINT and SPOT specular are NOT Blinn-Phong: the VU computes
//      `intensity^8` on the diffuse intensity (NdotL × atten) and uses that
//      as the specular factor. This produces a hot-spot near the light along
//      the diffuse axis rather than a viewer-dependent highlight. Only
//      PARALLEL lights use the halfway-vector NdotH model.
vec3 CalcPS2LitColor(vec4 normal, vec4 viewPos, vec3 vertexColor)
{
    vec3 N = normalize(normal.xyz);

    // Parallel-light specular exponent matches CalcIntens (sgexlit.vsm). The
    // VU squares NdotH once and the auxiliary intensity once more, approx NdotH^4.
    const float parallel_shininess = 4.0;

    // Material terms are already baked by sglight.c:SetMaterialData into
    // Parallel_Ambient, DColor, SColor, and point/spot btimes.
    vec3 vc = vertexColor + uAmbient.rgb;

    // Directional (parallel) lights: directions and halfway vectors are both
    // pre-rotated to view space by the renderer (mikupan_renderer.c:929-981).
    for (int i = 0; i < uParCount.x; i++)
    {
        float NdotL = max(dot(N, uParDir[i].xyz), 0.0);
        float NdotH = max(dot(N, uParHalfway[i].xyz), 0.0);

        // Diffuse: baked DColorMtx[i] * NdotL.
        vc += uParDiffuse[i].rgb * NdotL;
        // Specular: baked SColorMtx[i] * NdotH^n.
        vc += uParSpecular[i].rgb * pow(NdotH, parallel_shininess);
    }

    // Point lights: the VU dots the normal with an unnormalized light vector,
    // then multiplies by 1/dist^2, so the normalized shader equivalent is
    // NdotL * baked_btimes / dist.
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

        // Fake-Phong specular: intensity^8 (3 squarings in _CalcPointB chain).
        float spec = intensity * intensity;   // ^2
        spec *= spec;                         // ^4
        spec *= spec;                         // ^8
        vc += uPointSpecular[i].rgb * spec;
    }

    // Spot lights: asm_CalcSpotLight, with the same baked btimes / dist
    // intensity as points plus the VU cone gate.
    for (int i = 0; i < uSpotCount.x; i++)
    {
        vec3 L = uSpotPos[i].xyz - viewPos.xyz;       // vertex to light
        float dist2 = dot(L, L);
        if (dist2 <= 0.0)
        {
            continue;
        }

        float inv_dist = inversesqrt(dist2);
        vec3 Ldir = L * inv_dist;

        // Cone gate. uSpotDir is (pos - target), the cone-axis direction
        // pointing OUT of the cone. dot(Ldir, Z) where Ldir points to the
        // light is +1 on the cone axis at the light, falling off toward the
        // edge. cos^2(theta) in [intens..1] maps through intens_b to a ramp.
        // intens_b = 1/(1-intens) is uploaded in uSpotIntens.y.
        vec3 Z = normalize(uSpotDir[i].xyz);
        float cd = max(dot(Ldir, Z), 0.0);
        float cos2 = cd * cd;
        float gate = max(cos2 - uSpotIntens[i].x, 0.0) * uSpotIntens[i].y;
        gate = clamp(gate, 0.0, 1.0);

        if (cos2 <= uSpotIntens[i].x)
        {
            continue;
        }

        float NdotL = max(dot(N, Ldir), 0.0);
        float intensity = min(NdotL * uSpotPower[i].x * inv_dist, 1.0);

        vc += uSpotDiffuse[i].rgb * intensity * gate;

        float spec = intensity * intensity;   // ^2
        spec *= spec;                         // ^4
        spec *= spec;                         // ^8
        vc += uSpotSpecular[i].rgb * spec * gate;
    }

    return vc;
}

void main()
{
    vec4 color = texture(uTexture, vUV);

    if (renderNormals == 1)
    {
        FragColor = vNormal; //vec4(oVertexColor, 1.0f);
        return;
    }

    if (color.a == 0.0f)
    {
        discard;
    }

    if (staticLighting == 1)
    {
        FragColor = vec4(oVertexColor, 1.0f);
        return;
    }

    if (disableLighting == 1)
    {
        FragColor = color;
        return;
    }

    float materialAlpha = GetMaterialAlpha();
    color.a *= materialAlpha;

    // Lighting (or skip when toggled off via the UI checkbox).
    // This matches the VU1 vf18 model: it takes the per-vertex
    // colour and the texture colour separately, accumulates lighting onto the
    // vertex colour additively (matching `MADDA` chains in CalcIntens), and
    // performs the GS modulate (vertex_color × texture) at the end.
    vec3 ps2LightColor = uMeshLightingMode == 1
        ? oLitVertexColor
        : CalcPS2LitColor(vNormal, oViewPosition, oVertexColor);
    color.rgb = ApplyGsModulate(color.rgb, ps2LightColor);

    // Fog after lighting
    float fogFactor = uFog.w * (1.0 / -oViewPosition.z) + uFog.z;
    fogFactor = clamp(fogFactor, uFog.x, uFog.y);
    color.rgb = mix(uFogColor.rgb, color.rgb, fogFactor);

    // PS2-style projector shadow — sample the silhouette texture using the
    // shadow camera's world-clip-view. Anything outside the [0,1] square of
    // shadow UVs reads 0 (CLAMP_TO_BORDER), so fragments outside the caster's
    // bounding-box footprint stay untouched.
    if (uShadowEnabled == 1)
    {
        vec4 shadowClip = uShadowMatrix * oWorldPosition;
        if (shadowClip.w > 0.0)
        {
            vec3 sndc = shadowClip.xyz / shadowClip.w;
            vec2 suv  = sndc.xy * 0.5 + 0.5;

            // Sampling guard: only inside the projector frustum AND the
            // receiver must be in front of the light (sndc.z >= -1) — keeps
            // us from "casting up" onto fragments behind the light plane.
            if (suv.x >= 0.0 && suv.x <= 1.0 &&
                suv.y >= 0.0 && suv.y <= 1.0 &&
                sndc.z >= -1.0 && sndc.z <= 1.0)
            {
                float occluded = texture(uShadowTex, suv).r;
                color.rgb *= 1.0 - occluded * uShadowStrength;
            }
        }
    }

    FragColor = color;
}
