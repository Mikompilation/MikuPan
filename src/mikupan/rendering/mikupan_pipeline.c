#include "mikupan_pipeline.h"
#include "mikupan/mikupan_types.h"
#include <glad/gl.h>
#include <stdlib.h>

typedef enum
{
    MIKUPAN_RS_UNINIT = 0,
    MIKUPAN_RS_3D,
    MIKUPAN_RS_3D_MIRROR,
    MIKUPAN_RS_2D,
    MIKUPAN_RS_SPRITE_3D
} MikuPan_RenderStateMode;

/// GL bind dedup. The driver still validates every glBind*/glActiveTexture/
/// glViewport call even when nothing changes; redundant calls are pure
/// overhead. We shadow the bound state and skip the underlying GL call when
/// the requested value already matches.
///
/// Sentinels: 0 means "no buffer/texture/VAO bound" (the GL default) AND
/// "cache cold" — both correctly force the next bind through. -1 in the
/// viewport array forces the first SetViewport call after reset.
static GLuint g_bound_vao             = 0;
static GLuint g_bound_array_buffer    = 0;
static GLuint g_bound_element_buffer  = 0;
static GLenum g_active_texture_unit   = GL_TEXTURE0;
static GLuint g_bound_texture_2d      = 0;
static int    g_viewport[4]           = {-1, -1, -1, -1};
MikuPan_PipelineInfo pipelines[MAX_NUMBER_OF_PIPELINES] = {0};
static MikuPan_RenderStateMode g_current_render_state = MIKUPAN_RS_UNINIT;

void MikuPan_InitPipeline()
{
    ///////// MESH_0x12_SHADER /////////
    MikuPan_PipelineInfo* curr_pipeline = &pipelines[POSITION3_NORMAL3_UV2];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 3);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTEX + NORMALS
    /// POSITION ATTRIBUTE
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 2);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        3, 0,
        sizeof(float[6]), 0);

    /// NORMALS ATTRIBUTE
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1],
        3, 1,
        sizeof(float[6]), sizeof(float[3]));

    /// BUFFER 2: UVs
    /// TEXCOORD ATTRIBUTE
    MikuPan_SetBufferObjectInfo(
        &curr_pipeline->buffers[1],
        4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0],
        2, 2,
        sizeof(float[2]), 0);

    /// BUFFER 3: Vertex Color
    MikuPan_SetBufferObjectInfo(
        &curr_pipeline->buffers[2],
        4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[2].attributes[0],
        3, 3,
        sizeof(float[3]), 0);

    /// IBO: index buffer bound to this VAO for Core Profile glDrawElements
    glad_glGenBuffers(1, &curr_pipeline->ibo);
    glad_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, curr_pipeline->ibo);
    glad_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u_int) * 1024 * 1024, NULL, GL_DYNAMIC_DRAW);

    glad_glBindVertexArray(0);

    ///////// MESH_0x12_SHADER (SoA — for mesh_type 0x32 only) /////////
    curr_pipeline = &pipelines[POSITION3_NORMAL3_UV2_SOA];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 4);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 0: POSITIONS (vec3, contiguous)
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        3, 0,
        sizeof(float[3]), 0);

    /// BUFFER 1: NORMALS (vec3, contiguous)
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[1], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0],
        3, 1,
        sizeof(float[3]), 0);

    /// BUFFER 2: UVs (vec2)
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[2], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[2].attributes[0],
        2, 2,
        sizeof(float[2]), 0);

    /// BUFFER 3: VERTEX COLORS (vec3)
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[3], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[3].attributes[0],
        3, 3,
        sizeof(float[3]), 0);

    /// IBO
    glad_glGenBuffers(1, &curr_pipeline->ibo);
    glad_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, curr_pipeline->ibo);
    glad_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u_int) * 1024 * 1024, NULL, GL_DYNAMIC_DRAW);

    glad_glBindVertexArray(0);

    ///////// MESH_0x2_SHADER /////////
    curr_pipeline = &pipelines[POSITION4_NORMAL4_UV2];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 2);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTEX + NORMALS
    /// POSITION ATTRIBUTE
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 2);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        4, 0,
        sizeof(float[2][4]), 0);

    /// NORMALS ATTRIBUTE
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1],
        4, 1,
        sizeof(float[2][4]), sizeof(float[4]));

    /// BUFFER 2: UVs
    /// TEXCOORD ATTRIBUTE
    MikuPan_SetBufferObjectInfo(
        &curr_pipeline->buffers[1],
        4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0],
        2, 2,
        sizeof(float[2]), 0);

    /// IBO: index buffer bound to this VAO for Core Profile glDrawElements
    glad_glGenBuffers(1, &curr_pipeline->ibo);
    glad_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, curr_pipeline->ibo);
    glad_glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u_int) * 1024 * 1024, NULL, GL_DYNAMIC_DRAW);

    glad_glBindVertexArray(0);

    ///////// UNTEXTURED_COLOURED_SPRITE_SHADER /////////
    /// Buffers for line and square
    curr_pipeline = &pipelines[COLOUR4_POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: COLOURS + SPRITE POSITION
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(float[8]) * 4, 2);

    /// COLOUR
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        4, 0,
        sizeof(float[8]), 0);

    /// SPRITE POSITION
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1],
        4, 1,
        sizeof(float[8]), sizeof(float[4]));

    glad_glEnableVertexAttribArray(0);

    ///////// BOUNDING_BOX_SHADER /////////
    curr_pipeline = &pipelines[POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTICES
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(float[4]) * 24, 1);

    /// BB POSITION
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        4, 0,
        sizeof(float[4]), 0);

    glad_glEnableVertexAttribArray(0);

    ///////// SPRITE SHADER /////////
    curr_pipeline = &pipelines[UV4_COLOUR4_POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1 — sized for batched sprite rendering (~4096 sprites × 6 verts × 12 floats)
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4096 * 6 * 12 * (int)sizeof(float), 3);

    /// UV ATTRIBUTE
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        4, 0,
        sizeof(float[3][4]), 0);

    /// COLOR ATTRIBUTE
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1],
        4, 1,
        sizeof(float[3][4]), sizeof(float[4]));

    /// POSITION ATTRIBUTE
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[2],
        4, 2,
        sizeof(float[3][4]), sizeof(float[2][4]));

    ///////// LIGHT DATA BUFFER /////////
    curr_pipeline = &pipelines[LIGHTING_DATA];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);

    /// BUFFER 1
    MikuPan_SetUniformBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(MikuPan_LightData), 0);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glad_glBindBufferBase(GL_UNIFORM_BUFFER, 0, curr_pipeline->buffers[0].id);

    ///////// MATERIAL DATA BUFFER /////////
    // Per-material colours bound to indexed binding point 1, same pattern as
    // the LightBlock above.
    curr_pipeline = &pipelines[MATERIAL_DATA];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    MikuPan_SetUniformBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(MikuPan_MaterialData), 0);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glad_glBindBufferBase(GL_UNIFORM_BUFFER, 1, curr_pipeline->buffers[0].id);

    glad_glEnableVertexAttribArray(0);
}

void MikuPan_CreateBufferObjectsInfo(MikuPan_PipelineInfo* pipeline_info, u_int num_buffers)
{
    pipeline_info->num_buffers = num_buffers;
    pipeline_info->buffers = malloc(sizeof(MikuPan_BufferObjectInfo) * num_buffers);
}

void MikuPan_SetVertexBufferAttributeInfo(MikuPan_AttributeInfo *attribute_info,
                                    int size, u_int index, int stride,
                                    u_int offset)
{
    attribute_info->size = size;
    attribute_info->index = index;
    attribute_info->stride = stride;
    attribute_info->offset = offset;



    glad_glEnableVertexAttribArray(attribute_info->index);
    glad_glVertexAttribPointer(
        attribute_info->index,
        attribute_info->size,
        GL_FLOAT, GL_FALSE,
        attribute_info->stride,
        (void *) attribute_info->offset);
}

void MikuPan_SetUniformBufferObjectInfo(MikuPan_BufferObjectInfo *object_info,
                                int buffer_length, u_int num_attributes)
{
    glad_glGenBuffers(1, &object_info->id);

    object_info->buffer_length = buffer_length;
    object_info->num_attributes = num_attributes;

    if (num_attributes > 0)
    {
        object_info->attributes = malloc(sizeof(MikuPan_AttributeInfo) * object_info->num_attributes);
    }
    else
    {
        object_info->attributes = NULL;
    }

    glad_glBindBuffer(GL_UNIFORM_BUFFER, object_info->id);
    glad_glBufferData(
        GL_UNIFORM_BUFFER,
        object_info->buffer_length,
        NULL,
        GL_DYNAMIC_DRAW);
}

void MikuPan_SetBufferObjectInfo(MikuPan_BufferObjectInfo *object_info,
                                int buffer_length, u_int num_attributes)
{
    glad_glGenBuffers(1, &object_info->id);

    object_info->buffer_length = buffer_length;
    object_info->num_attributes = num_attributes;

    if (num_attributes > 0)
    {
        object_info->attributes = malloc(sizeof(MikuPan_AttributeInfo) * object_info->num_attributes);
    }

    glad_glBindBuffer(GL_ARRAY_BUFFER, object_info->id);
    glad_glBufferData(
        GL_ARRAY_BUFFER,
        object_info->buffer_length,
        NULL,
        GL_DYNAMIC_DRAW);
}

MikuPan_PipelineInfo* MikuPan_GetPipelineInfo(enum MikuPan_PipelineType pipeline_type)
{
    if (pipeline_type == MAX_NUMBER_OF_PIPELINES)
    {
        return (void*)0;
    }

    return &pipelines[pipeline_type];
}

void MikuPan_SetRenderState3D()
{
    if (g_current_render_state == MIKUPAN_RS_3D) return;
    g_current_render_state = MIKUPAN_RS_3D;

    glad_glDepthMask(GL_TRUE);
    glad_glEnable(GL_CULL_FACE);
    //glad_glCullFace(GL_BACK);
    glad_glEnable(GL_DEPTH_TEST);
    glad_glDepthFunc(GL_LEQUAL);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void MikuPan_SetRenderState3DMirror()
{
    if (g_current_render_state == MIKUPAN_RS_3D_MIRROR) return;
    g_current_render_state = MIKUPAN_RS_3D_MIRROR;

    glad_glDepthMask(GL_TRUE);
    glad_glEnable(GL_CULL_FACE);
    glad_glCullFace(GL_FRONT);
    glad_glEnable(GL_DEPTH_TEST);
    glad_glDepthFunc(GL_LEQUAL);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void MikuPan_SetRenderState2D()
{
    if (g_current_render_state == MIKUPAN_RS_2D) return;
    g_current_render_state = MIKUPAN_RS_2D;

    glad_glDisable(GL_CULL_FACE);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glad_glDisable(GL_DEPTH_TEST);
}

void MikuPan_SetRenderStateSprite3D()
{
    if (g_current_render_state == MIKUPAN_RS_SPRITE_3D) return;
    g_current_render_state = MIKUPAN_RS_SPRITE_3D;

    glad_glDisable(GL_CULL_FACE);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glad_glEnable(GL_DEPTH_TEST);
    glad_glDepthFunc(GL_LEQUAL);
}

void MikuPan_SetTriangleStripRestart()
{
    glad_glEnable(GL_PRIMITIVE_RESTART);
    glad_glPrimitiveRestartIndex(-1);
}

void MikuPan_ResetRenderStateCache(void)
{
    g_current_render_state = MIKUPAN_RS_UNINIT;
}

int MikuPan_GetCurrentRenderStateMode(void)
{
    return (int)g_current_render_state;
}

void MikuPan_ApplyRenderStateMode(int mode)
{
    switch ((MikuPan_RenderStateMode)mode)
    {
        case MIKUPAN_RS_3D:        MikuPan_SetRenderState3D();        break;
        case MIKUPAN_RS_3D_MIRROR: MikuPan_SetRenderState3DMirror();  break;
        case MIKUPAN_RS_2D:        MikuPan_SetRenderState2D();        break;
        case MIKUPAN_RS_SPRITE_3D: MikuPan_SetRenderStateSprite3D();  break;
        default:                   MikuPan_SetRenderState3D();        break;
    }
}

void MikuPan_BindVAO(GLuint vao)
{
    if (vao != g_bound_vao)
    {
        glad_glBindVertexArray(vao);
        g_bound_vao = vao;

        /// GL_ELEMENT_ARRAY_BUFFER is VAO state. A different VAO may carry a
        /// different IBO even if our global shadow still names the old buffer.
        g_bound_element_buffer = 0;
    }
}

void MikuPan_BindBufferCached(GLenum target, GLuint buffer)
{
    if (target == GL_ARRAY_BUFFER)
    {
        if (buffer != g_bound_array_buffer)
        {
            glad_glBindBuffer(target, buffer);
            g_bound_array_buffer = buffer;
        }
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
    {
        if (buffer != g_bound_element_buffer)
        {
            glad_glBindBuffer(target, buffer);
            g_bound_element_buffer = buffer;
        }
    }
    else
    {
        // Other targets (UBO/copy/etc.) are rare here; pass through uncached.
        glad_glBindBuffer(target, buffer);
    }
}

void MikuPan_ActiveTextureCached(GLenum unit)
{
    if (unit != g_active_texture_unit)
    {
        glad_glActiveTexture(unit);
        g_active_texture_unit = unit;
    }
}

void MikuPan_BindTexture2DCached(GLuint texture)
{
    // The codebase only ever samples from unit 0; ensure it's active so the
    // shadowed g_bound_texture_2d stays coherent with the actual GL state
    // even if external code (ImGui) switched units. The cache check on the
    // ActiveTexture path makes this near-free in steady state.
    MikuPan_ActiveTextureCached(GL_TEXTURE0);

    if (texture != g_bound_texture_2d)
    {
        glad_glBindTexture(GL_TEXTURE_2D, texture);
        g_bound_texture_2d = texture;
    }
}

void MikuPan_SetViewportCached(int x, int y, int w, int h)
{
    if (x != g_viewport[0] || y != g_viewport[1] ||
        w != g_viewport[2] || h != g_viewport[3])
    {
        glad_glViewport(x, y, w, h);
        g_viewport[0] = x;
        g_viewport[1] = y;
        g_viewport[2] = w;
        g_viewport[3] = h;
    }
}

void MikuPan_ResetGLBindCache(void)
{
    // ImGui (and other external GL clients) clobber bound state. Force the
    // next call into each cached binder to actually issue the GL call.
    g_bound_vao             = 0;
    g_bound_array_buffer    = 0;
    g_bound_element_buffer  = 0;
    g_active_texture_unit   = 0; // 0 isn't a valid GL texture unit — guarantees re-issue
    g_bound_texture_2d      = 0;
    g_viewport[0] = -1;
    g_viewport[1] = -1;
    g_viewport[2] = -1;
    g_viewport[3] = -1;
}

GLuint MikuPan_GetBoundVAO(void)
{
    return g_bound_vao;
}

GLuint MikuPan_GetBoundTexture2D(void)
{
    return g_bound_texture_2d;
}
