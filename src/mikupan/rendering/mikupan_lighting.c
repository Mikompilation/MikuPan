#include "mikupan_renderer_internal.h"
#include "mikupan_pipeline.h"
#include "mikupan/mikupan_types.h"
#include <string.h>

#define MIKUPAN_MAX_LIGHTS 3

static MikuPan_LightData mikupan_light_data = {0};
static MikuPan_MaterialData mikupan_material_data = {
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
};

static int   g_raw_point_count = 0;
static int   g_raw_spot_count  = 0;
static float g_raw_point_pos[MIKUPAN_MAX_LIGHTS][4] = {0};
static float g_raw_spot_pos[MIKUPAN_MAX_LIGHTS][4] = {0};
static float g_raw_spot_dir[MIKUPAN_MAX_LIGHTS][4] = {0};

static int MikuPan_ClampLightCount(int count)
{
    if (count < 0)
    {
        return 0;
    }

    return count > MIKUPAN_MAX_LIGHTS ? MIKUPAN_MAX_LIGHTS : count;
}

static float MikuPan_Ps2ColorToUnit(float color)
{
    // VU lighting is accumulated/clamped in a 0..255 color bus. The shader
    // keeps that as normalized 0..1, then applies the GS MODULATE 255/128
    // scale when combining with the texture.
    return color / 255.0f;
}

static float MikuPan_Ps2AlphaToUnit(float alpha)
{
    if (alpha <= 0.0f)
    {
        return 0.0f;
    }

    if (alpha >= 128.0f)
    {
        return 1.0f;
    }

    return alpha / 128.0f;
}

static void MikuPan_CopyVec4(float dst[4], const float src[4])
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

static void MikuPan_CopyBakedColorVec4(float dst[4], const float src[4])
{
    dst[0] = MikuPan_Ps2ColorToUnit(src[0]);
    dst[1] = MikuPan_Ps2ColorToUnit(src[1]);
    dst[2] = MikuPan_Ps2ColorToUnit(src[2]);
    dst[3] = MikuPan_Ps2ColorToUnit(src[3]);
}

static void MikuPan_CopyBakedColorMatrix(float dst[MIKUPAN_MAX_LIGHTS][4],
                                         const float (*src)[4],
                                         int slot_count)
{
    memset(dst, 0, sizeof(float[MIKUPAN_MAX_LIGHTS][4]));

    if (src == NULL)
    {
        return;
    }

    slot_count = MikuPan_ClampLightCount(slot_count);
    for (int i = 0; i < slot_count; i++)
    {
        MikuPan_CopyBakedColorVec4(dst[i], src[i]);
    }
}

static void MikuPan_CopySelectedGeometry(float dst[MIKUPAN_MAX_LIGHTS][4],
                                         const float src[MIKUPAN_MAX_LIGHTS][4],
                                         int src_count,
                                         const int lnum[MIKUPAN_MAX_LIGHTS],
                                         int slot_count)
{
    memset(dst, 0, sizeof(float[MIKUPAN_MAX_LIGHTS][4]));
    slot_count = MikuPan_ClampLightCount(slot_count);

    for (int i = 0; i < slot_count; i++)
    {
        int src_idx = lnum != NULL ? lnum[i] : i;

        if (src_idx < 0 || src_idx >= src_count)
        {
            continue;
        }

        MikuPan_CopyVec4(dst[i], src[src_idx]);
    }
}

static void MikuPan_CopyBakedPower(float dst[MIKUPAN_MAX_LIGHTS][4],
                                   const float src[4],
                                   int slot_count)
{
    memset(dst, 0, sizeof(float[MIKUPAN_MAX_LIGHTS][4]));

    if (src == NULL)
    {
        return;
    }

    slot_count = MikuPan_ClampLightCount(slot_count);
    for (int i = 0; i < slot_count; i++)
    {
        dst[i][0] = src[i];
    }
}

static void MikuPan_CopySpotIntens(float dst[MIKUPAN_MAX_LIGHTS][4],
                                   const float intens[4],
                                   const float intens_b[4],
                                   int slot_count)
{
    memset(dst, 0, sizeof(float[MIKUPAN_MAX_LIGHTS][4]));

    if (intens == NULL || intens_b == NULL)
    {
        return;
    }

    slot_count = MikuPan_ClampLightCount(slot_count);
    for (int i = 0; i < slot_count; i++)
    {
        dst[i][0] = intens[i];
        dst[i][1] = intens_b[i];
    }
}

static void MikuPan_UploadLightData(void)
{
    MikuPan_PipelineInfo *p = MikuPan_GetPipelineInfo(LIGHTING_DATA);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, p->buffers[0].id);
    glad_glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MikuPan_LightData),
                         &mikupan_light_data);
}

void MikuPan_SetupAmbientLighting(const LIGHT_PACK *lp, float *eyevec)
{
    // Populate the entire LightBlock UBO straight from the LIGHT_PACK input.
    // The previous design relied on MikuPan_SetupAmbientLighting2() reading
    // colors from SgLightParallelp, but that pointer is updated per-material
    // by SetMaterialData (sglight.c:473) and is stale/zero at the SetLightData
    // call site — which is why the scene was so dark: the shader was getting
    // empty diffuse colors. Source the canonical values directly here.

    memset(&mikupan_light_data, 0, sizeof(mikupan_light_data));
    memset(g_raw_point_pos, 0, sizeof(g_raw_point_pos));
    memset(g_raw_spot_pos, 0, sizeof(g_raw_spot_pos));
    memset(g_raw_spot_dir, 0, sizeof(g_raw_spot_dir));
    g_raw_point_count = 0;
    g_raw_spot_count = 0;
    mikupan_light_data.uMaterialAlpha[0] = 1.0f;

    // Scene ambient.
    mikupan_light_data.uAmbient[0] = lp->ambient[0];
    mikupan_light_data.uAmbient[1] = lp->ambient[1];
    mikupan_light_data.uAmbient[2] = lp->ambient[2];
    mikupan_light_data.uAmbient[3] = lp->ambient[3];

    mat3 view3;
    glm_mat4_pick3(WorldView, view3);

    // Parallel (directional) lights. Compute the same Blinn-Phong halfway
    // vector that SgSetDefaultLight (sglight.c:118-120) bakes into SLightMtx:
    //     halfway = normalize(ieye + dir)   where ieye = -normalize(eye)
    vec3 ieye_ws = {-eyevec[0], -eyevec[1], -eyevec[2]};
    glm_vec3_normalize(ieye_ws);

    vec3 ieye_vs;
    glm_mat3_mulv(view3, ieye_ws, ieye_vs);
    glm_vec3_normalize(ieye_vs);

    int parCount = MikuPan_ClampLightCount(lp->parallel_num);
    mikupan_light_data.uParCount[0] = parCount;

    for (int i = 0; i < parCount; i++)
    {
        // Direction is in world space; rotate (no translation) into view space
        // so the shader can dot it against view-space normals directly.
        vec4 dirWS = {
            lp->parallel[i].direction[0], lp->parallel[i].direction[1],
            lp->parallel[i].direction[2], lp->parallel[i].direction[3]};
        vec4 dirVS;
        glm_mat3_mulv(view3, dirWS, dirVS);
        vec3 dirVSn = {dirVS[0], dirVS[1], dirVS[2]};
        glm_vec3_normalize(dirVSn);

        mikupan_light_data.uParDir[i][0] = dirVSn[0];
        mikupan_light_data.uParDir[i][1] = dirVSn[1];
        mikupan_light_data.uParDir[i][2] = dirVSn[2];
        mikupan_light_data.uParDir[i][3] = 1.0f;

        mikupan_light_data.uParDiffuse[i][0] = lp->parallel[i].diffuse[0];
        mikupan_light_data.uParDiffuse[i][1] = lp->parallel[i].diffuse[1];
        mikupan_light_data.uParDiffuse[i][2] = lp->parallel[i].diffuse[2];
        mikupan_light_data.uParDiffuse[i][3] = lp->parallel[i].diffuse[3];

        // light_pack carries no separate specular field for parallel lights;
        // gra3d.c:1083 mirrors specular = diffuse on the SgLIGHT side, so
        // match that convention here.
        mikupan_light_data.uParSpecular[i][0] = lp->parallel[i].diffuse[0];
        mikupan_light_data.uParSpecular[i][1] = lp->parallel[i].diffuse[1];
        mikupan_light_data.uParSpecular[i][2] = lp->parallel[i].diffuse[2];
        mikupan_light_data.uParSpecular[i][3] = lp->parallel[i].diffuse[3];

        // Halfway vector for Blinn-Phong — matches sglight.c:118-120 exactly.
        vec3 halfway = {ieye_vs[0] + dirVSn[0], ieye_vs[1] + dirVSn[1],
                        ieye_vs[2] + dirVSn[2]};
        glm_vec3_normalize(halfway);
        mikupan_light_data.uParHalfway[i][0] = halfway[0];
        mikupan_light_data.uParHalfway[i][1] = halfway[1];
        mikupan_light_data.uParHalfway[i][2] = halfway[2];
        mikupan_light_data.uParHalfway[i][3] = 1.0f;
    }

    // Point lights.
    int pointCount = MikuPan_ClampLightCount(lp->point_num);
    g_raw_point_count = pointCount;
    mikupan_light_data.uPointCount[0] = pointCount;

    for (int i = 0; i < pointCount; i++)
    {
        vec4 posWS = {lp->point[i].pos[0], lp->point[i].pos[1],
                      lp->point[i].pos[2], 1.0f};
        vec4 posVS;
        glm_mat4_mulv(WorldView, posWS, posVS);

        mikupan_light_data.uPointPos[i][0] = posVS[0];
        mikupan_light_data.uPointPos[i][1] = posVS[1];
        mikupan_light_data.uPointPos[i][2] = posVS[2];
        mikupan_light_data.uPointPos[i][3] = 1.0f;
        MikuPan_CopyVec4(g_raw_point_pos[i], mikupan_light_data.uPointPos[i]);

        mikupan_light_data.uPointDiffuse[i][0] = lp->point[i].diffuse[0];
        mikupan_light_data.uPointDiffuse[i][1] = lp->point[i].diffuse[1];
        mikupan_light_data.uPointDiffuse[i][2] = lp->point[i].diffuse[2];
        mikupan_light_data.uPointDiffuse[i][3] = lp->point[i].diffuse[3];

        // Specular = diffuse, mirroring gra3d.c:1101.
        mikupan_light_data.uPointSpecular[i][0] = lp->point[i].diffuse[0];
        mikupan_light_data.uPointSpecular[i][1] = lp->point[i].diffuse[1];
        mikupan_light_data.uPointSpecular[i][2] = lp->point[i].diffuse[2];
        mikupan_light_data.uPointSpecular[i][3] = lp->point[i].diffuse[3];

        mikupan_light_data.uPointPower[i][0] = lp->point[i].power;
    }

    // Spot lights.
    int spotCount = MikuPan_ClampLightCount(lp->spot_num);
    g_raw_spot_count = spotCount;
    mikupan_light_data.uSpotCount[0] = spotCount;

    for (int i = 0; i < spotCount; i++)
    {
        vec4 posWS = {lp->spot[i].pos[0], lp->spot[i].pos[1],
                      lp->spot[i].pos[2], 1.0f};
        vec4 posVS;
        glm_mat4_mulv(WorldView, posWS, posVS);

        mikupan_light_data.uSpotPos[i][0] = posVS[0];
        mikupan_light_data.uSpotPos[i][1] = posVS[1];
        mikupan_light_data.uSpotPos[i][2] = posVS[2];
        mikupan_light_data.uSpotPos[i][3] = 1.0f;
        MikuPan_CopyVec4(g_raw_spot_pos[i], mikupan_light_data.uSpotPos[i]);

        vec4 dirWS = {lp->spot[i].direction[0], lp->spot[i].direction[1],
                      lp->spot[i].direction[2], lp->spot[i].direction[3]};
        vec4 dirVS;
        glm_mat3_mulv(view3, dirWS, dirVS);
        glm_vec3_normalize(dirVS);

        mikupan_light_data.uSpotDir[i][0] = dirVS[0];
        mikupan_light_data.uSpotDir[i][1] = dirVS[1];
        mikupan_light_data.uSpotDir[i][2] = dirVS[2];
        mikupan_light_data.uSpotDir[i][3] = 1.0f;
        MikuPan_CopyVec4(g_raw_spot_dir[i], mikupan_light_data.uSpotDir[i]);

        mikupan_light_data.uSpotDiffuse[i][0] = lp->spot[i].diffuse[0];
        mikupan_light_data.uSpotDiffuse[i][1] = lp->spot[i].diffuse[1];
        mikupan_light_data.uSpotDiffuse[i][2] = lp->spot[i].diffuse[2];
        mikupan_light_data.uSpotDiffuse[i][3] = lp->spot[i].diffuse[3];

        // Specular = diffuse, mirroring gra3d.c:1122.
        mikupan_light_data.uSpotSpecular[i][0] = lp->spot[i].diffuse[0];
        mikupan_light_data.uSpotSpecular[i][1] = lp->spot[i].diffuse[1];
        mikupan_light_data.uSpotSpecular[i][2] = lp->spot[i].diffuse[2];
        mikupan_light_data.uSpotSpecular[i][3] = lp->spot[i].diffuse[3];

        mikupan_light_data.uSpotPower[i][0] = lp->spot[i].power;//1250

        // Cone gate parameters. Match SgSetSpotLights (sglight.c:914):
        //   intens   = cos²(half-angle), the inner-cone threshold
        //   intens_b = 1/(1 - intens), the per-spot reciprocal used by
        //              asm_CalcSpotLight (sglight.c:1125-1127) to normalize
        //              cone² ∈ [intens..1] back to a 0..1 falloff ramp.
        // intens == 1.0 is degenerate (zero-width cone) — clamp away from it
        // so we don't divide by zero. The shader ignores the spot anyway when
        // cone² <= intens, so the clamp is purely a numerical guard.
        float intens = lp->spot[i].intens;
        float intens_b = 0.0f;
        if (intens < 0.9999f)
        {
            intens_b = 1.0f / (1.0f - intens);
        }
        mikupan_light_data.uSpotIntens[i][0] = intens;
        mikupan_light_data.uSpotIntens[i][1] = intens_b;
    }

    // The LightBlock UBO is bound to indexed binding point 0 via
    // glBindBufferBase at init, but glBufferSubData(GL_UNIFORM_BUFFER, ...)
    // reads from the *generic* GL_UNIFORM_BUFFER target — which was left at
    // 0 (unbound) after pipeline init. Without this bind the upload was a
    // silent no-op and the shader saw zeros for diffuse colors / counts,
    // leaving the scene effectively unlit.
    MikuPan_UploadLightData();
}

void MikuPan_SetBakedLighting(int parallel_count,
                              const sceVu0FVECTOR parallel_ambient,
                              const float (*parallel_dcolor)[4],
                              const float (*parallel_scolor)[4],
                              int point_group_count,
                              const int point_lnum[3],
                              const float (*point_dcolor)[4],
                              const float (*point_scolor)[4],
                              const sceVu0FVECTOR point_btimes,
                              int spot_group_count,
                              const int spot_lnum[3],
                              const float (*spot_dcolor)[4],
                              const float (*spot_scolor)[4],
                              const sceVu0FVECTOR spot_btimes,
                              const sceVu0FVECTOR spot_intens,
                              const sceVu0FVECTOR spot_intens_b)
{
    int parCount = MikuPan_ClampLightCount(parallel_count);
    int pointSlots = point_group_count > 0 ? MIKUPAN_MAX_LIGHTS : 0;
    int spotSlots = spot_group_count > 0 ? MIKUPAN_MAX_LIGHTS : 0;

    if (parallel_ambient != NULL)
    {
        MikuPan_CopyBakedColorVec4(mikupan_light_data.uAmbient,
                                   parallel_ambient);
    }

    mikupan_light_data.uParCount[0] = parCount;
    MikuPan_CopyBakedColorMatrix(mikupan_light_data.uParDiffuse,
                                 parallel_dcolor, parCount);
    MikuPan_CopyBakedColorMatrix(mikupan_light_data.uParSpecular,
                                 parallel_scolor, parCount);
    mikupan_light_data.uMaterialAlpha[0] =
        parallel_dcolor != NULL
            ? MikuPan_Ps2AlphaToUnit(parallel_dcolor[0][3])
            : 1.0f;

    mikupan_light_data.uPointCount[0] = pointSlots;
    MikuPan_CopySelectedGeometry(mikupan_light_data.uPointPos,
                                 g_raw_point_pos, g_raw_point_count,
                                 point_lnum, pointSlots);
    MikuPan_CopyBakedColorMatrix(mikupan_light_data.uPointDiffuse,
                                 point_dcolor, pointSlots);
    MikuPan_CopyBakedColorMatrix(mikupan_light_data.uPointSpecular,
                                 point_scolor, pointSlots);
    MikuPan_CopyBakedPower(mikupan_light_data.uPointPower,
                           point_btimes, pointSlots);

    mikupan_light_data.uSpotCount[0] = spotSlots;
    MikuPan_CopySelectedGeometry(mikupan_light_data.uSpotPos,
                                 g_raw_spot_pos, g_raw_spot_count,
                                 spot_lnum, spotSlots);
    MikuPan_CopySelectedGeometry(mikupan_light_data.uSpotDir,
                                 g_raw_spot_dir, g_raw_spot_count,
                                 spot_lnum, spotSlots);
    MikuPan_CopyBakedColorMatrix(mikupan_light_data.uSpotDiffuse,
                                 spot_dcolor, spotSlots);
    MikuPan_CopyBakedColorMatrix(mikupan_light_data.uSpotSpecular,
                                 spot_scolor, spotSlots);
    MikuPan_CopyBakedPower(mikupan_light_data.uSpotPower,
                           spot_btimes, spotSlots);
    MikuPan_CopySpotIntens(mikupan_light_data.uSpotIntens,
                           spot_intens, spot_intens_b, spotSlots);

    MikuPan_UploadLightData();
}

void MikuPan_SetMaterial(const sceVu0FVECTOR *ambient,
                         const sceVu0FVECTOR *diffuse,
                         const sceVu0FVECTOR *specular,
                         const sceVu0FVECTOR *emission)
{
    memcpy(mikupan_material_data.uMatAmbient,  *ambient,  sizeof(float[4]));
    memcpy(mikupan_material_data.uMatDiffuse,  *diffuse,  sizeof(float[4]));
    memcpy(mikupan_material_data.uMatSpecular, *specular, sizeof(float[4]));
    memcpy(mikupan_material_data.uMatEmission, *emission, sizeof(float[4]));
    MikuPan_PipelineInfo *p = MikuPan_GetPipelineInfo(MATERIAL_DATA);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, p->buffers[0].id);
    glad_glBufferSubData(GL_UNIFORM_BUFFER, 0,
                         sizeof(MikuPan_MaterialData),
                         &mikupan_material_data);
}
