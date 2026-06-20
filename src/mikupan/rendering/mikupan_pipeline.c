#include "mikupan_pipeline.h"

#include "mikupan/mikupan_types.h"
#include "mikupan_gpu.h"
#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

typedef struct MikuPanPosition3Normal3Vertex
{
    float position[3];
    float normal[3];
} MikuPanPosition3Normal3Vertex;

typedef struct MikuPanPosition4Normal4Vertex
{
    float position[4];
    float normal[4];
} MikuPanPosition4Normal4Vertex;

typedef struct MikuPanColour4Position4Vertex
{
    float colour[4];
    float position[4];
} MikuPanColour4Position4Vertex;

typedef struct MikuPanSpriteVertex
{
    float uv[4];
    float colour[4];
    float position[4];
} MikuPanSpriteVertex;

typedef struct MikuPanSkinnedVertex
{
    float bone_pos0[4];
    float bone_pos1[4];
    float bone_norm0[4];
    float bone_norm1[4];
} MikuPanSkinnedVertex;

typedef enum
{
    MIKUPAN_RS_UNINIT = 0,
    MIKUPAN_RS_3D,
    MIKUPAN_RS_3D_MIRROR,
    MIKUPAN_RS_2D,
    MIKUPAN_RS_2D_DEPTH,
    MIKUPAN_RS_SPRITE_3D,
    MIKUPAN_RS_SHADOW,
    MIKUPAN_RS_SHADOW_RECEIVER
} MikuPan_RenderStateMode;

static u_int g_bound_vao = 0;
static u_int g_bound_array_buffer = 0;
static u_int g_bound_element_buffer = 0;
static u_int g_active_texture_unit = GL_TEXTURE0;
static u_int g_bound_texture_2d[2] = {0, 0};
static int g_viewport[4] = {-1, -1, -1, -1};

MikuPan_PipelineInfo pipelines[MAX_NUMBER_OF_PIPELINES] = {0};
static MikuPan_RenderStateMode g_current_render_state = MIKUPAN_RS_UNINIT;

static void MikuPan_FinalizePipeline(enum MikuPan_PipelineType type)
{
    MikuPan_PipelineInfo *pipeline = &pipelines[type];
    unsigned int buffers[4] = {0};
    for (u_int i = 0; i < pipeline->num_buffers && i < 4; i++)
    {
        buffers[i] = pipeline->buffers[i].id;
    }
    pipeline->vao = MikuPan_GPURegisterVertexArray(
        type, pipeline->num_buffers, buffers, pipeline->ibo);
}

void MikuPan_InitPipeline()
{
    MikuPan_PipelineInfo *curr_pipeline = NULL;

    ///////// MESH_0x12_SHADER /////////
    curr_pipeline = &pipelines[POSITION3_NORMAL3_UV2];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 3);

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 2);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 3, 0,
        (int) sizeof(MikuPanPosition3Normal3Vertex),
        (u_int) offsetof(MikuPanPosition3Normal3Vertex, position));
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1], 3, 1,
        (int) sizeof(MikuPanPosition3Normal3Vertex),
        (u_int) offsetof(MikuPanPosition3Normal3Vertex, normal));

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[1], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0], 2, 2,
        sizeof(float[2]), 0);

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[2], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[2].attributes[0], 3, 3,
        sizeof(float[3]), 0);

    curr_pipeline->ibo = MikuPan_GPUCreateBuffer(
        sizeof(u_int) * 1024 * 1024, MIKUPAN_GPU_BUFFER_INDEX);
    MikuPan_FinalizePipeline(POSITION3_NORMAL3_UV2);

    ///////// MESH_0x12_SHADER (SoA - for mesh_type 0x32 only) /////////
    curr_pipeline = &pipelines[POSITION3_NORMAL3_UV2_SOA];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 4);

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 3, 0,
        sizeof(float[3]), 0);

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[1], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0], 3, 1,
        sizeof(float[3]), 0);

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[2], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[2].attributes[0], 2, 2,
        sizeof(float[2]), 0);

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[3], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[3].attributes[0], 3, 3,
        sizeof(float[3]), 0);

    curr_pipeline->ibo = MikuPan_GPUCreateBuffer(
        sizeof(u_int) * 1024 * 1024, MIKUPAN_GPU_BUFFER_INDEX);
    MikuPan_FinalizePipeline(POSITION3_NORMAL3_UV2_SOA);

    ///////// MESH_0x2_SHADER /////////
    curr_pipeline = &pipelines[POSITION4_NORMAL4_UV2];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 2);

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 2);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 4, 0,
        (int) sizeof(MikuPanPosition4Normal4Vertex),
        (u_int) offsetof(MikuPanPosition4Normal4Vertex, position));
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1], 4, 1,
        (int) sizeof(MikuPanPosition4Normal4Vertex),
        (u_int) offsetof(MikuPanPosition4Normal4Vertex, normal));

    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[1], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0], 2, 2,
        sizeof(float[2]), 0);

    curr_pipeline->ibo = MikuPan_GPUCreateBuffer(
        sizeof(u_int) * 1024 * 1024, MIKUPAN_GPU_BUFFER_INDEX);
    MikuPan_FinalizePipeline(POSITION4_NORMAL4_UV2);

    ///////// GPU-SKINNED MESH 0x2 / 0xA /////////
    curr_pipeline = &pipelines[SKIN_POSITION4X2_NORMAL4X2_UV2];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 2);

    /// buffer0: four float4 per vertex (bone-0 pos+weight, bone-1 pos+indices,
    /// bone-0 normal+normal-weight, bone-1 normal). Stride = sizeof(float[4][4]).
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 4);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 4, 0,
        (int) sizeof(MikuPanSkinnedVertex),
        (u_int) offsetof(MikuPanSkinnedVertex, bone_pos0));

    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1], 4, 1,
        (int) sizeof(MikuPanSkinnedVertex),
        (u_int) offsetof(MikuPanSkinnedVertex, bone_pos1));

    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[2], 4, 2,
        (int) sizeof(MikuPanSkinnedVertex),
        (u_int) offsetof(MikuPanSkinnedVertex, bone_norm0));

    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[3], 4, 3,
        (int) sizeof(MikuPanSkinnedVertex),
        (u_int) offsetof(MikuPanSkinnedVertex, bone_norm1));

    /// buffer1: float2 UV (attribute location 4).
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[1], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0], 2, 4,
        sizeof(float[2]), 0);

    curr_pipeline->ibo = MikuPan_GPUCreateBuffer(
        sizeof(u_int) * 1024 * 1024, MIKUPAN_GPU_BUFFER_INDEX);
    MikuPan_FinalizePipeline(SKIN_POSITION4X2_NORMAL4X2_UV2);

    ///////// UNTEXTURED_COLOURED_SPRITE_SHADER /////////
    curr_pipeline = &pipelines[COLOUR4_POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    MikuPan_SetBufferObjectInfo(
        &curr_pipeline->buffers[0], 4096 * 8 * (int)sizeof(float), 2);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 4, 0,
        (int) sizeof(MikuPanColour4Position4Vertex),
        (u_int) offsetof(MikuPanColour4Position4Vertex, colour));
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1], 4, 1,
        (int) sizeof(MikuPanColour4Position4Vertex),
        (u_int) offsetof(MikuPanColour4Position4Vertex, position));
    MikuPan_FinalizePipeline(COLOUR4_POSITION4);

    ///////// BOUNDING_BOX_SHADER /////////
    curr_pipeline = &pipelines[POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    MikuPan_SetBufferObjectInfo(
        &curr_pipeline->buffers[0], sizeof(float[4]) * 24, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 4, 0,
        sizeof(float[4]), 0);
    MikuPan_FinalizePipeline(POSITION4);

    ///////// SHADOW SILHOUETTE POSITION STREAM /////////
    curr_pipeline = &pipelines[SHADOW_POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 4 * 1024 * 1024, 1);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 4, 0,
        sizeof(float[4]), 0);
    curr_pipeline->ibo = MikuPan_GPUCreateBuffer(
        sizeof(u_int) * 1024 * 1024, MIKUPAN_GPU_BUFFER_INDEX);
    MikuPan_FinalizePipeline(SHADOW_POSITION4);

    ///////// SPRITE SHADER /////////
    curr_pipeline = &pipelines[UV4_COLOUR4_POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    MikuPan_SetBufferObjectInfo(
        &curr_pipeline->buffers[0],
        4096 * 6 * 12 * (int)sizeof(float), 3);
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0], 4, 0,
        (int) sizeof(MikuPanSpriteVertex),
        (u_int) offsetof(MikuPanSpriteVertex, uv));
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1], 4, 1,
        (int) sizeof(MikuPanSpriteVertex),
        (u_int) offsetof(MikuPanSpriteVertex, colour));
    MikuPan_SetVertexBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[2], 4, 2,
        (int) sizeof(MikuPanSpriteVertex),
        (u_int) offsetof(MikuPanSpriteVertex, position));
    MikuPan_FinalizePipeline(UV4_COLOUR4_POSITION4);

    ///////// LIGHT DATA BUFFER /////////
    curr_pipeline = &pipelines[LIGHTING_DATA];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    MikuPan_SetUniformBufferObjectInfo(
        &curr_pipeline->buffers[0], sizeof(MikuPan_LightData), 0);

    ///////// MATERIAL DATA BUFFER /////////
    curr_pipeline = &pipelines[MATERIAL_DATA];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    MikuPan_SetUniformBufferObjectInfo(
        &curr_pipeline->buffers[0], sizeof(MikuPan_MaterialData), 0);
}

void MikuPan_CreateBufferObjectsInfo(MikuPan_PipelineInfo *pipeline_info,
                                     u_int num_buffers)
{
    pipeline_info->num_buffers = num_buffers;
    pipeline_info->buffers =
        calloc(num_buffers, sizeof(MikuPan_BufferObjectInfo));
}

void MikuPan_SetVertexBufferAttributeInfo(MikuPan_AttributeInfo *attribute_info,
                                          int size, u_int index, int stride,
                                          u_int offset)
{
    attribute_info->size = size;
    attribute_info->index = index;
    attribute_info->stride = stride;
    attribute_info->offset = offset;
}

void MikuPan_SetUniformBufferObjectInfo(MikuPan_BufferObjectInfo *object_info,
                                        int buffer_length, u_int num_attributes)
{
    object_info->id =
        MikuPan_GPUCreateUniformCPUBuffer((unsigned int)buffer_length);
    object_info->buffer_length = buffer_length;
    object_info->num_attributes = num_attributes;
    object_info->attributes =
        num_attributes > 0 ?
            calloc(num_attributes, sizeof(MikuPan_AttributeInfo)) :
            NULL;
}

void MikuPan_SetBufferObjectInfo(MikuPan_BufferObjectInfo *object_info,
                                 int buffer_length, u_int num_attributes)
{
    object_info->id =
        MikuPan_GPUCreateBuffer((unsigned int)buffer_length,
                                MIKUPAN_GPU_BUFFER_VERTEX);
    object_info->buffer_length = buffer_length;
    object_info->num_attributes = num_attributes;
    object_info->attributes =
        num_attributes > 0 ?
            calloc(num_attributes, sizeof(MikuPan_AttributeInfo)) :
            NULL;
}

MikuPan_PipelineInfo *MikuPan_GetPipelineInfo(enum MikuPan_PipelineType pipeline_type)
{
    if (pipeline_type == MAX_NUMBER_OF_PIPELINES)
    {
        return NULL;
    }

    return &pipelines[pipeline_type];
}

void MikuPan_SetRenderState3D()
{
    if (g_current_render_state == MIKUPAN_RS_3D) return;
    g_current_render_state = MIKUPAN_RS_3D;
    MikuPan_GPUSetRenderState3D();
}

void MikuPan_SetRenderState3DMirror()
{
    if (g_current_render_state == MIKUPAN_RS_3D_MIRROR) return;
    g_current_render_state = MIKUPAN_RS_3D_MIRROR;
    MikuPan_GPUSetRenderState3DMirror();
}

void MikuPan_SetRenderState2D()
{
    if (g_current_render_state == MIKUPAN_RS_2D) return;
    g_current_render_state = MIKUPAN_RS_2D;
    MikuPan_GPUSetRenderState2D();
}

void MikuPan_SetRenderState2DDepth()
{
    if (g_current_render_state == MIKUPAN_RS_2D_DEPTH) return;
    g_current_render_state = MIKUPAN_RS_2D_DEPTH;
    MikuPan_GPUSetRenderState2DDepth();
}

void MikuPan_SetRenderStateSprite3D()
{
    if (g_current_render_state == MIKUPAN_RS_SPRITE_3D) return;
    g_current_render_state = MIKUPAN_RS_SPRITE_3D;
    MikuPan_GPUSetRenderStateSprite3D();
}

void MikuPan_SetRenderStateShadow()
{
    if (g_current_render_state == MIKUPAN_RS_SHADOW) return;
    g_current_render_state = MIKUPAN_RS_SHADOW;
    MikuPan_GPUSetRenderStateShadow();
}

void MikuPan_SetRenderStateShadowReceiver()
{
    if (g_current_render_state == MIKUPAN_RS_SHADOW_RECEIVER) return;
    g_current_render_state = MIKUPAN_RS_SHADOW_RECEIVER;
    MikuPan_GPUSetRenderStateShadowReceiver();
}

void MikuPan_SetTriangleStripRestart()
{
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
        case MIKUPAN_RS_2D_DEPTH:  MikuPan_SetRenderState2DDepth();   break;
        case MIKUPAN_RS_SPRITE_3D: MikuPan_SetRenderStateSprite3D();  break;
        case MIKUPAN_RS_SHADOW:    MikuPan_SetRenderStateShadow();    break;
        case MIKUPAN_RS_SHADOW_RECEIVER:
            MikuPan_SetRenderStateShadowReceiver();
            break;
        default:                   MikuPan_SetRenderState3D();        break;
    }
}

void MikuPan_BindVAO(u_int vao)
{
    if (vao != g_bound_vao)
    {
        MikuPan_GPUBindVertexArray(vao);
        g_bound_vao = vao;
        g_bound_element_buffer = 0;
    }
}

void MikuPan_BindBufferCached(u_int target, u_int buffer)
{
    if (target == GL_ARRAY_BUFFER)
    {
        g_bound_array_buffer = buffer;
    }
    else if (target == GL_ELEMENT_ARRAY_BUFFER)
    {
        g_bound_element_buffer = buffer;
    }
}

void MikuPan_ActiveTextureCached(u_int unit)
{
    g_active_texture_unit = unit;
}

void MikuPan_BindTexture2DCached(u_int texture)
{
    int slot = 0;
    if (g_active_texture_unit >= GL_TEXTURE0)
    {
        slot = (int)(g_active_texture_unit - GL_TEXTURE0);
    }
    if (slot < 0 || slot > 1)
    {
        slot = 0;
    }

    if (texture != g_bound_texture_2d[slot])
    {
        MikuPan_GPUBindTextureSlot(slot, texture);
        g_bound_texture_2d[slot] = texture;
    }
}

void MikuPan_SetViewportCached(int x, int y, int w, int h)
{
    if (x != g_viewport[0] || y != g_viewport[1] ||
        w != g_viewport[2] || h != g_viewport[3])
    {
        MikuPan_GPUSetViewport(x, y, w, h);
        g_viewport[0] = x;
        g_viewport[1] = y;
        g_viewport[2] = w;
        g_viewport[3] = h;
    }
}

void MikuPan_ResetGLBindCache(void)
{
    g_bound_vao = 0;
    g_bound_array_buffer = 0;
    g_bound_element_buffer = 0;
    g_active_texture_unit = GL_TEXTURE0;
    g_bound_texture_2d[0] = 0;
    g_bound_texture_2d[1] = 0;
    g_viewport[0] = -1;
    g_viewport[1] = -1;
    g_viewport[2] = -1;
    g_viewport[3] = -1;
}

u_int MikuPan_GetBoundVAO(void)
{
    return g_bound_vao;
}

u_int MikuPan_GetBoundTexture2D(void)
{
    return g_bound_texture_2d[0];
}
