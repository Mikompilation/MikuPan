#ifndef MIKUPAN_MIKUPAN_BASICTYPES_H
#define MIKUPAN_MIKUPAN_BASICTYPES_H

#include "typedefs.h"
#include <SDL3/SDL_gpu.h>

typedef struct
{
    float x;
    float y;
    float w;
    float h;
} MikuPan_Rect;

typedef struct
{
    int              width;
    int              height;
    SDL_GPUTexture  *texture;   // GPU texture handle (replaces GLuint id)
    uint64_t         tex0;
    uint64_t         hash;
} MikuPan_TextureInfo;

// Attribute descriptor (kept for pipeline vertex layout description)
typedef struct
{
    int    size;    // component count: 2, 3 or 4
    u_int  index;   // vertex attribute location
    int    stride;  // bytes between vertices
    u_int  offset;  // byte offset within vertex
} MikuPan_AttributeInfo;

// Buffer descriptor – id is now a SDL_GPUBuffer* (vertex / index)
typedef struct
{
    SDL_GPUBuffer        *buffer;         // GPU-side buffer
    int                   buffer_length;  // allocation size in bytes
    u_int                 num_attributes;
    MikuPan_AttributeInfo *attributes;
} MikuPan_BufferObjectInfo;

// Pipeline descriptor – the SDL_GPUGraphicsPipeline is owned by the
// pipeline module; this struct is kept for vertex-layout bookkeeping.
typedef struct
{
    SDL_GPUGraphicsPipeline *pipeline; // SDL_GPU pipeline (replaces VAO)
    u_int                    num_buffers;
    MikuPan_BufferObjectInfo *buffers;
} MikuPan_PipelineInfo;

enum MikuPan_PipelineType
{
    /// BOUNDING BOX SHADER
    POSITION4,

    /// MESH 0x12 SHADER
    POSITION3_NORMAL3_UV2,

    /// MESH 0x2 / 0xA SHADER
    POSITION4_NORMAL4_UV2,

    /// SPRITE (textured 2-D)
    UV4_COLOUR4_POSITION4,

    /// UNTEXTURED COLOURED SPRITE / LINES
    COLOUR4_POSITION4,

    /// Lighting uniform data (no longer a separate buffer pipeline in SDL_GPU;
    /// kept as sentinel for MAX_NUMBER_OF_PIPELINES compatibility)
    LIGHTING_DATA,

    MAX_NUMBER_OF_PIPELINES
};

#endif /* MIKUPAN_MIKUPAN_BASICTYPES_H */
