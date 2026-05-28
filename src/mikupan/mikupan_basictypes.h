#ifndef MIKUPAN_MIKUPAN_BASICTYPES_H
#define MIKUPAN_MIKUPAN_BASICTYPES_H
#include "typedefs.h"

typedef struct
{
    u_int id;
} MikuPan_RenderBufferObject;

typedef struct
{
    u_int id;
} MikuPan_FrameBufferObject;

typedef struct
{
    int width;
    int height;
    u_int id;
    uint64_t tex0;
    uint64_t hash;
} MikuPan_TextureInfo;

typedef struct
{
    int msaa;
    MikuPan_FrameBufferObject framebuffer;
    MikuPan_RenderBufferObject depth;
    MikuPan_RenderBufferObject colour;
    MikuPan_TextureInfo texture;
    MikuPan_RenderBufferObject framebuffer_readback;
} MikuPan_MsaaBufferObject;

typedef struct
{
    float x;
    float y;
    float w;
    float h;
} MikuPan_Rect;

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
    u_int ibo;
    u_int num_buffers;
    MikuPan_BufferObjectInfo* buffers;
} MikuPan_PipelineInfo;

typedef struct
{
    int width;
    int height;
} MikuPan_Resolution;

typedef struct
{
    MikuPan_Resolution window;
    MikuPan_Resolution render;
    int is_fullscreen;
    int vsync;
    int lighting_mode;
    int msaa_index;
    float brightness;
    float gamma;
} MikuPan_ConfigRenderer;

typedef struct
{
    int enabled;
    float strength;
    float curvature;
    float overscan;
    float scanline_strength;
    float scanline_scale;
    float scanline_thickness;
    float mask_strength;
    float mask_scale;
    float vignette_strength;
    float vignette_size;
    float chroma_offset;
    float blend_strength;
    float blend_radius;
    float noise_strength;
    float flicker_strength;
    float glow_strength;
} MikuPan_ConfigCrt;

typedef struct
{
    MikuPan_ConfigRenderer renderer;
    MikuPan_ConfigCrt crt;
    int selected_theme;
} MikuPan_Config;

enum MikuPan_PipelineType
{
    /// BOUNDING BOX SHADER
    POSITION4,

    /// MESH 0x12 SHADER
    POSITION3_NORMAL3_UV2,

    /// MESH 0x12 SHADER (SoA layout — used by mesh_type 0x32 only).
    /// Same shader & attribute locations as POSITION3_NORMAL3_UV2 but
    /// positions / normals / UVs / colors live in 4 separate VBOs so that
    /// 0x32 uploads can avoid the per-vertex CPU interleaving step.
    POSITION3_NORMAL3_UV2_SOA,

    /// MESH 0x2 SHADER
    POSITION4_NORMAL4_UV2,

    /// SPRITE
    UV4_COLOUR4_POSITION4,

    /// UNTEXTURED COLOURED SPRITE
    COLOUR4_POSITION4,

    /// Lighting Data
    LIGHTING_DATA,

    /// Per-material colour block (Ambient/Diffuse/Specular/Emission). Pushed
    /// by MikuPan_SetMaterial when SetMaterialData fires for a new material.
    MATERIAL_DATA,

    /// THE MAXIMUM NUM OF PIPELINES, USEFUL FOR LOOPS
    MAX_NUMBER_OF_PIPELINES
};

#endif//MIKUPAN_MIKUPAN_BASICTYPES_H
