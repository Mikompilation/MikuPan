#include "mikupan_pipeline.h"

#include <glad/gl.h>
#include <stdlib.h>

/// DEFAULT_SHADER
//GLuint VAO, VBO, uvVBO = 0;

/// MESH_0x12_SHADER
//GLuint MESH_0x12_VAO, MESH_0x12_VN_VBO, MESH_0x12_UV_VBO = 0;
//GLuint gSpriteVAO, gSpriteVBO = 0;
//GLuint gShapeVAO, gShapeVBO = 0;
//GLuint gBBVAO, gBBVBO = 0;

MikuPan_PipelineInfo pipelines[MAX_NUMBER_OF_PIPELINES] = {0};

void MikuPan_InitPipeline()
{
    ///////// DEFAULT_SHADER /////////
    MikuPan_PipelineInfo* curr_pipeline = &pipelines[POSITION3];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// POSITION ATTRIBUTE
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 1024 * 32, 1);
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        3, 0,
        sizeof(float[3]), 0);

    glad_glBindVertexArray(0);

    ///////// MESH_0x12_SHADER /////////
    curr_pipeline = &pipelines[POSITION3_NORMAL3_UV];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 2);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTEX + NORMALS
    /// POSITION ATTRIBUTE
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 1024 * 32, 2);
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        3, 0,
        sizeof(float[6]), 0);

    /// NORMALS ATTRIBUTE
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1],
        3, 1,
        sizeof(float[3]), sizeof(float[3]));

    /// BUFFER 2: UVs
    /// TEXCOORD ATTRIBUTE
    MikuPan_SetBufferObjectInfo(
        &curr_pipeline->buffers[1],
        1024 * 32, 1);
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0],
        2, 2,
        sizeof(float[2]), 0);

    glad_glBindVertexArray(0);

    ///////// UI_SPRITE_SHADER /////////
    curr_pipeline = &pipelines[POSITION2_UV];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTICES + UVs
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(float[6][4]), 2);

    /// SPRITE POSITION
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        2, 0,
        sizeof(float[4]), 0);

    /// SPRITE UV
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1],
        2, 1,
        sizeof(float[4]), sizeof(float[2]));

    glad_glBindVertexArray(0);

    ///////// UNTEXTURED_SPRITE_SHADER /////////
    /// Buffers for line and square
    curr_pipeline = &pipelines[POSITION2];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTICES
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(float[2]) * 6, 1);

    /// SPRITE POSITION
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        2, 0,
        sizeof(float[2]), 0);

    glad_glEnableVertexAttribArray(0);

    ///////// BOUNDING_BOX_SHADER /////////
    curr_pipeline = &pipelines[POSITION4];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTICES
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(float[4]) * 24, 1);

    /// BB POSITION
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        4, 0,
        sizeof(float[4]), 0);

    glad_glEnableVertexAttribArray(0);

    ///////// SIMPLE TEXTURED SHADER /////////
    curr_pipeline = &pipelines[POSITION3_UV];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 2);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1: VERTICES -> POSITION
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], 1024 * 32, 1);

    /// POSITION ATTRIBUTE
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        3, 0,
        sizeof(float[3]), 0);

    /// BUFFER 2: UVs
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[1], 1024 * 32, 1);

    /// UVs ATTRIBUTE
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[1].attributes[0],
        2, 1,
        sizeof(float[2]), 0);

    glad_glEnableVertexAttribArray(0);

    ///////// SPRITE 3D SHADER /////////
    curr_pipeline = &pipelines[POSITION3_UV_3D];
    MikuPan_CreateBufferObjectsInfo(curr_pipeline, 1);
    glad_glGenVertexArrays(1, &curr_pipeline->vao);
    glad_glBindVertexArray(curr_pipeline->vao);

    /// BUFFER 1
    MikuPan_SetBufferObjectInfo(&curr_pipeline->buffers[0], sizeof(float[4][3][4]), 3);

    /// UV ATTRIBUTE
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[0],
        4, 0,
        sizeof(float[3][4]), 0);

    /// COLOR ATTRIBUTE
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[1],
        4, 1,
        sizeof(float[3][4]), sizeof(float[4]));

    /// POSITION ATTRIBUTE
    MikuPan_SetBufferAttributeInfo(
        &curr_pipeline->buffers[0].attributes[2],
        4, 2,
        sizeof(float[3][4]), sizeof(float[2][4]));

    glad_glEnableVertexAttribArray(0);
}

void MikuPan_CreateBufferObjectsInfo(MikuPan_PipelineInfo* pipeline_info, u_int num_buffers)
{
    pipeline_info->num_buffers = num_buffers;
    pipeline_info->buffers = malloc(sizeof(MikuPan_BufferObjectInfo) * num_buffers);
}

void MikuPan_SetBufferAttributeInfo(MikuPan_AttributeInfo *attribute_info,
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

void MikuPan_SetBufferObjectInfo(MikuPan_BufferObjectInfo *object_info,
                                int buffer_length, u_int num_attributes)
{
    glad_glGenBuffers(1, &object_info->id);
    object_info->buffer_length = buffer_length;
    object_info->num_attributes = num_attributes;
    object_info->attributes = malloc(sizeof(MikuPan_AttributeInfo) * object_info->num_attributes);
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