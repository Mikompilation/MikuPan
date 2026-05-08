#ifndef MIKUPAN_MIKUPAN_PIPELINE_H
#define MIKUPAN_MIKUPAN_PIPELINE_H

#include "mikupan/mikupan_basictypes.h"

void MikuPan_InitPipeline();
void MikuPan_CreateBufferObjectsInfo(MikuPan_PipelineInfo* pipeline_info, u_int num_buffers);
void MikuPan_SetBufferObjectInfo(MikuPan_BufferObjectInfo* object_info,  int buffer_length,  u_int num_attributes);
void MikuPan_SetVertexBufferAttributeInfo(MikuPan_AttributeInfo* attribute_info, int size, u_int index, int stride, u_int offset);
MikuPan_PipelineInfo* MikuPan_GetPipelineInfo(enum MikuPan_PipelineType pipeline_type);
void MikuPan_SetUniformBufferObjectInfo(MikuPan_BufferObjectInfo *object_info,
                                int buffer_length, u_int num_attributes);
void MikuPan_SetRenderState3D();
void MikuPan_SetRenderState3DMirror();
void MikuPan_SetRenderState2D();
void MikuPan_SetRenderStateSprite3D();
void MikuPan_SetTriangleStripRestart();
void MikuPan_ResetRenderStateCache(void);

/// The cached-mesh queue snapshots the active render-state mode at push time
/// and restores it per-entry at flush time. Without this, mirror reflections
/// (cull-front, RS_3D_MIRROR) get drawn with whatever mode is active when the
/// queue eventually drains — usually plain RS_3D — and look broken.
int  MikuPan_GetCurrentRenderStateMode(void);
void MikuPan_ApplyRenderStateMode(int mode);

/// Cached GL bind helpers. Each compares the requested value to a shadowed
/// "currently bound" tracker and skips the underlying GL call when unchanged.
/// Call MikuPan_ResetGLBindCache() after any external code (ImGui, etc.) may
/// have mutated GL state, so the next call into these helpers re-issues.
void MikuPan_BindVAO(u_int vao);
void MikuPan_BindBufferCached(u_int target, u_int buffer);
void MikuPan_ActiveTextureCached(u_int unit);
void MikuPan_BindTexture2DCached(u_int texture);
void MikuPan_SetViewportCached(int x, int y, int w, int h);
void MikuPan_ResetGLBindCache(void);

/// Read-only accessors for the cached bind state — used by the draw-call
/// inspector to record which VAO/texture each draw was using without paying
/// for a glGetIntegerv round-trip.
u_int MikuPan_GetBoundVAO(void);
u_int MikuPan_GetBoundTexture2D(void);

#endif//MIKUPAN_MIKUPAN_PIPELINE_H
