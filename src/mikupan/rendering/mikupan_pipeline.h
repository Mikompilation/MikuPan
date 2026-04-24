#ifndef MIKUPAN_MIKUPAN_PIPELINE_H
#define MIKUPAN_MIKUPAN_PIPELINE_H

#include "mikupan/mikupan_basictypes.h"
#include <SDL3/SDL_gpu.h>

// ─────────────────────────────────────────────────────────────────────────────
// SDL_GPU pipeline variants
// Render state is baked into each pipeline object, so solid/wireframe/mirror
// and 2D/3D combinations each require a distinct SDL_GPUGraphicsPipeline.
// ─────────────────────────────────────────────────────────────────────────────
typedef enum
{
    // Mesh 0x12 — interleaved pos3+norm3, separate uv2
    GPU_PIPELINE_MESH_0x12_SOLID,
    GPU_PIPELINE_MESH_0x12_WIREFRAME,
    GPU_PIPELINE_MESH_0x12_MIRROR,

    // Mesh 0x2 — interleaved pos4+norm4, separate uv2 (model matrix applied)
    GPU_PIPELINE_MESH_0x2_SOLID,
    GPU_PIPELINE_MESH_0x2_WIREFRAME,
    GPU_PIPELINE_MESH_0x2_MIRROR,

    // Mesh 0xA — same vertex layout as 0x2 but pre-transformed (no model matrix)
    GPU_PIPELINE_MESH_0xA_SOLID,
    GPU_PIPELINE_MESH_0xA_WIREFRAME,
    GPU_PIPELINE_MESH_0xA_MIRROR,

    // Sprite (uv4+colour4+pos4) — 2D (no depth) and 3D (with depth)
    GPU_PIPELINE_SPRITE_2D,
    GPU_PIPELINE_SPRITE_2D_WIREFRAME,
    GPU_PIPELINE_SPRITE_3D,

    // Untextured coloured (colour4+pos4)
    GPU_PIPELINE_UNLIT_STRIP_2D,   // triangle strip, no depth
    GPU_PIPELINE_LINE_2D,          // line list, no depth

    // Bounding box (pos4) — line strip, depth test, no blend
    GPU_PIPELINE_BOUNDING_BOX,

    // Final blit from offscreen render target → swapchain (no depth attachment)
    GPU_PIPELINE_BLIT_TO_SWAPCHAIN,

    MAX_GPU_PIPELINES
} MikuPan_GpuPipelineType;

// ─────────────────────────────────────────────────────────────────────────────
// Shared GPU resources
// ─────────────────────────────────────────────────────────────────────────────

// Anisotropic linear sampler used for all textured draws
extern SDL_GPUSampler *g_gpu_sampler;

// Shared index buffer (4 MB, INT32) for all indexed mesh draws
extern SDL_GPUBuffer  *g_index_buffer;

// Legacy-compatible pipeline info array indexed by MikuPan_PipelineType.
// Each entry carries the vertex SDL_GPUBuffer handles and layout metadata
// (stride, offset) used by the renderer for transfer-buffer uploads.
// The .pipeline field is intentionally unused — use MikuPan_GetGpuPipeline().
extern MikuPan_PipelineInfo g_pipelines[MAX_NUMBER_OF_PIPELINES];

// ─────────────────────────────────────────────────────────────────────────────
// API
// ─────────────────────────────────────────────────────────────────────────────

// Creates all pipelines, vertex/index buffers, and the sampler.
// color_format / sample_count must match the render target created by the
// renderer; call MikuPan_RecreatePipelines after an MSAA/format change.
int  MikuPan_InitPipelines(SDL_GPUDevice       *device,
                           SDL_GPUTextureFormat  color_format,
                           SDL_GPUSampleCount    sample_count);

void MikuPan_DestroyPipelines(SDL_GPUDevice *device);

// Destroys and recreates only the pipeline objects (not vertex/index buffers).
int  MikuPan_RecreatePipelines(SDL_GPUDevice       *device,
                               SDL_GPUTextureFormat  color_format,
                               SDL_GPUSampleCount    sample_count);

// Returns the SDL_GPUGraphicsPipeline for the given variant.
SDL_GPUGraphicsPipeline *MikuPan_GetGpuPipeline(MikuPan_GpuPipelineType type);

// Returns the buffer/attribute info for a vertex layout (legacy-compatible).
MikuPan_PipelineInfo    *MikuPan_GetPipelineInfo(enum MikuPan_PipelineType pipeline_type);

#endif /* MIKUPAN_MIKUPAN_PIPELINE_H */
