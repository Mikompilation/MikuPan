#ifndef MIKUPAN_MIKUPAN_BASICTYPES_H
#define MIKUPAN_MIKUPAN_BASICTYPES_H
#include "typedefs.h"

typedef struct
{
    float x;
    float y;
    float w;
    float h;
} MikuPan_Rect;

typedef struct
{
    int width;
    int height;
    u_int id;
    unsigned char* data;
} MikuPan_TextureInfo;

typedef struct
{
    /// Size in glad_glVertexAttribPointer
    int size;
    u_int index;
    int stride;
    u_int offset;
} MikuPan_AttributeInfo;

typedef struct
{
    u_int id;
    /// Size in glad_glBufferData
    int buffer_length;
    u_int num_attributes;
    MikuPan_AttributeInfo* attributes;
} MikuPan_BufferObjectInfo;

typedef struct
{
    u_int vao;
    u_int num_buffers;
    MikuPan_BufferObjectInfo* buffers;
} MikuPan_PipelineInfo;

enum MikuPan_PipelineType
{
    /// DEFAULT SHADER
    POSITION3,

    /// BOUNDING BOX SHADER
    POSITION4,

    /// MESH 0x12 SHADER
    POSITION3_NORMAL3_UV,

    /// SIMPLE TEXTURED SHADER
    POSITION3_UV,

    /// UI SPRITE
    POSITION2_UV,

    /// UNTEXTURED SPRITE
    POSITION2,

    /// THE MAXIMUM NUM OF PIPELINES, USEFUL FOR LOOPS
    MAX_NUMBER_OF_PIPELINES
};

#endif//MIKUPAN_MIKUPAN_BASICTYPES_H
