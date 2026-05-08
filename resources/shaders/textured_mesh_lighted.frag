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
};

// Per-material colours (Ambient/Diffuse/Specular/Emission). Pushed by
// MikuPan_SetMaterial when sglight.c:SetMaterialData fires for a new material,
// mirroring the vectors that the original PS2 path bakes into Parallel_Ambient,
// Parallel_DColor and Parallel_SColor. .w of uMatSpecular is reused as a
// shininess factor (1..255 → 4..256 exponent), preserving alpha for material
// magnitude tracking via .w of the diffuse on the original side.
layout(std140) uniform MaterialBlock
{
    vec4 uMatAmbient;
    vec4 uMatDiffuse;
    vec4 uMatSpecular;
    vec4 uMatEmission;
};

/// Input variables from vertex shader
in vec2 vUV;
in vec4 vNormal;
in vec4 oViewPosition;
in vec4 oWorldPosition;
in vec3 oVertexColor;

out vec4 FragColor;

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

/// Per-mesh-type lighting selector. The PS2 path splits pre-baking across the
/// six standard mesh types:
///
///   * 0x10 / 0x12 / 0x32 — preset/static meshes go through gra3d.c::SgPreRender
///     which calls sglight.c::SetPreRenderTYPE0/2/2F. Those iterate the prim
///     packet and bake CalcPointLight() + CalcSpotLight() into the per-vertex
///     colour slot (sglight.c:1498/1576/1680 write back into prim[0..2]).
///     The VU then adds only parallel + ambient at draw time. Our renderer
///     reads the same baked colour via pVMCD->avColor, so to match the PS2
///     we MUST skip dynamic point/spot in the shader for these types.
///
///   * 0x2 / 0xA / 0x82 — skinned/dynamic and NVL meshes have NO CPU pre-bake
///     (SetPreRenderMeshData switch only covers 0x10/0x12/0x32). At draw the
///     VU's CalcSpotPoint dispatch (sgsuv.vsm D_EL_*) does parallel + spot +
///     point per-vertex on top of an authored or zero base colour. We must
///     run the full dynamic lighting in the shader.
///
/// uMeshLightingMode encodes which path to take:
///   0 = full dynamic (parallel + point + spot + ambient)  → 0x2/0xA/0x82
///   1 = pre-baked dynamic (parallel + ambient only)       → 0x10/0x12/0x32
///
/// Setting this incorrectly is what made bright rooms look blown-out before:
/// 0x12 meshes were getting point+spot doubled (once baked in oVertexColor,
/// once added by the shader).
uniform int uMeshLightingMode;

/// Fog uniforms (kept as regular uniforms)
uniform vec4 uFog;      // x=min, y=max, z=base, w=scale
uniform vec4 uFogColor;

uniform float uColorScale;

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
//       L_to_v   = vert - lightPos                       //   light → vertex
//       inv_d2   = 1 / |L_to_v|²
//       NdotL_b  = max(rotated_lDir · L_to_v, 0) × btimes  // btimes = power×max_color
//       I        = min(NdotL_b × inv_d2, 1)              //   diffuse intensity, clamped
//       vf18 += DColor[i] × I                            // diffuse
//       vf18 += SColor[i] × I^8                          // FAKE-PHONG specular: pow8 of
//                                                        // diffuse intensity, NOT NdotH.
//
//   for each SPOT light i:                               // asm_CalcSpotLight (sglight.c:1079)
//       cosθ²  = (WDLightMtx · L_to_v)² × inv_d2         //   cone-axis alignment
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
vec3 ApplyPS2Lights(vec4 normal, vec4 viewPos, vec3 vertexColor, vec3 textureColor)
{
    vec3 N = normalize(normal.xyz);
    vec3 V = normalize(-viewPos.xyz); // camera at origin in view space

    // Parallel-light specular exponent matches CalcIntens (sgexlit.vsm). The
    // VU squares NdotH once and the auxiliary intensity once more, ≈ NdotH^4.
    const float parallel_shininess = 4.0;

    // Ambient term — sglight.c:505-508 bakes Parallel_Ambient as
    //     (TAmbient.xyz * matAmbient + matEmission) * TAmbient[3]   (=255 for primtype 0)
    // and then the GS combiner does `pixel = (Cv * Ct) >> 7` — that >> 7 (not
    // >> 8) is the implicit MODULATE-2× that lifts the ambient term so it
    // roughly matches the texture brightness in shadow. With diffuse/specular,
    // the contributions are usually large enough that the saturate clamp
    // masks the missing 2×, but ambient-only fragments end up half as bright
    // as the PS2 reference. Apply the same 2× pairing here so the GS-side
    // arithmetic is reproduced — this is the scaling the user sees baked into
    // graph3d (TAmbient[3] = 255) once you account for the >>7 modulate.
    vec3 ambient_term = (uAmbient.rgb * uMatAmbient.rgb + uMatEmission.rgb) * 2.0;
    vec3 vc = vertexColor + ambient_term;

    // Directional (parallel) lights — directions and halfway vectors are both
    // pre-rotated to view space by the renderer (mikupan_renderer.c:929-981).
    for (int i = 0; i < uParCount.x; i++)
    {
        float NdotL = max(dot(N, uParDir[i].xyz), 0.0);
        float NdotH = max(dot(N, uParHalfway[i].xyz), 0.0);

        // Diffuse: DColorMtx[i] × NdotL = (lightDiffuse × matDiffuse) × NdotL
        vc += uParDiffuse[i].rgb * uMatDiffuse.rgb * NdotL;
        // Specular: SColorMtx[i] × NdotH^n = (lightSpec × matSpec) × NdotH^n
        vc += uParSpecular[i].rgb * uMatSpecular.rgb * pow(NdotH, parallel_shininess);
    }

    // Point lights — _CalcPointA/B (sglight.c:1024-1049).
    //
    // Linear `power / dist` attenuation. The PS2 VU computes `1/dist²` paired
    // with normalisation factors (`max_color`, `DColorMtx[0][3]`, GS 8-bit
    // saturation) we don't model in this normalised-[0,1] pipeline; raw
    // `power / dist²` collapses contributions to invisibility at scene-typical
    // distances. Linear atten with the VU's `min(intensity, 1.0)` saturation
    // cap matches the visible falloff curve of the LIGHT_PACK values that
    // gra3d.c hands us.
    //
    // Known limitation: lights authored with tiny per-channel diffuse and
    // very large `power` (the player flashlight at diffuse≈0.00022 and
    // power=7e6 is the only one in this engine) can't reach a visible
    // contribution through this path because the final `diffuse × intensity`
    // multiply caps at the diffuse magnitude. Those lights are visible in
    // scenes via SgPreRender's per-vertex bake (gra3d.c, scene.c) and
    // currently fall through to nothing in gameplay; fixing it requires the
    // PS2's max_color normalisation upstream of this shader.
    //
    // Skip when uMeshLightingMode == 1: see uniform comment above.
    for (int i = 0; i < uPointCount.x; i++)
    {
        vec3 L = uPointPos[i].xyz - viewPos.xyz;
        float dist2 = dot(L, L);
        if (dist2 <= 0.0) continue;
        float inv_dist = inversesqrt(dist2);
        vec3 Ldir = L * inv_dist;
        float NdotL = max(dot(N, Ldir), 0.0);

        float intensity = min(NdotL * uPointPower[i].x * inv_dist, 1.0);

        vc += uPointDiffuse[i].rgb * uMatDiffuse.rgb * intensity;

        // Fake-Phong specular: intensity^8 (3 squarings in _CalcPointB chain).
        float spec = intensity * intensity;   // ^2
        spec *= spec;                         // ^4
        spec *= spec;                         // ^8
        vc += uPointSpecular[i].rgb * uMatSpecular.rgb * spec;
    }

    // Spot lights — asm_CalcSpotLight (sglight.c:1079-1158).
    // Smooth cone gate via intens_b ramp, the same linear attenuation as
    // points, and the same fake-Phong specular.
    for (int i = 0; i < uSpotCount.x; i++)
    {
        vec3 L = uSpotPos[i].xyz - viewPos.xyz;       // vert → light (= -L_to_v)
        float dist2 = dot(L, L);
        if (dist2 <= 0.0) continue;
        float inv_dist = inversesqrt(dist2);
        vec3 Ldir = L * inv_dist;

        // Cone gate. uSpotDir is (pos - target), the cone-axis direction
        // pointing OUT of the cone. dot(Ldir, Z) where Ldir points to the
        // light is +1 on the cone axis at the light, falling off toward the
        // edge. cos²θ ∈ [intens..1]; map it through intens_b to a 0..1 ramp.
        // intens_b = 1/(1-intens) is uploaded in uSpotIntens.y.
        vec3 Z = normalize(uSpotDir[i].xyz);
        float cd = max(dot(Ldir, Z), 0.0);
        float cos2 = cd * cd;
        float gate = max(cos2 - uSpotIntens[i].x, 0.0) * uSpotIntens[i].y;
        gate = clamp(gate, 0.0, 1.0);
        if (cos2 <= uSpotIntens[i].x) continue;

        float NdotL = max(dot(N, Ldir), 0.0);
        float intensity = min(NdotL * uSpotPower[i].x * inv_dist, 1.0);
        float len = uSpotPower[i].x / sqrt(dist2) /* * 0.25f */;

        vc += uSpotDiffuse[i].rgb * len * uMatDiffuse.rgb * intensity * gate;

        float spec = intensity * intensity;   // ^2
        spec *= spec;                         // ^4
        spec *= spec;                         // ^8
        vc += uSpotSpecular[i].rgb * uMatSpecular.rgb * spec * gate;
    }

    // GS MODULATE: pixel = texture × vertex_color (clipped to [0,1]).
    // vertex_color may exceed 1.0 (lighting can saturate); clamp before mod
    // mirrors the GS clamp on its 8-bit colour bus.
    return clamp(vc, vec3(0.0), vec3(1.0)) * textureColor;
}

void main()
{
    vec4 color = texture(uTexture, vUV);

    if (disableLighting == 1)
    {
        FragColor = color;
        return;
    }

    //if (color.a == 0.0)
    //{
    //    discard;
    //}

    vec3 albedo = color.rgb * oVertexColor;

    // Fog
    float fogFactor = uFog.w * (1.0 / -oViewPosition.z) + uFog.z;
    fogFactor = clamp(fogFactor, uFog.x, uFog.y);

    // Lighting (or skip when toggled off via the UI checkbox).
    // ApplyPS2Lights now matches the VU1 vf18 model: it takes the per-vertex
    // colour and the texture colour separately, accumulates lighting onto the
    // vertex colour additively (matching `MADDA` chains in CalcIntens), and
    // performs the GS modulate (vertex_color × texture) at the end.
    color.rgb = ApplyPS2Lights(vNormal, oViewPosition, oVertexColor, color.rgb);

    // Fog after lighting
    if (oVertexColor.rgb == vec3(0.0f, 0.0f, 0.0f))
    {
        color = mix(uFogColor, color, fogFactor);
    }


    if (renderNormals == 1)
    {
        FragColor = vec4(oVertexColor, 1.0f);
        return;
    }

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