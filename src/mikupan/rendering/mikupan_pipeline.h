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
void MikuPan_SetRenderState3DDoubleSided();
void MikuPan_SetRenderState2D();
void MikuPan_SetRenderState2DDepth();
void MikuPan_SetRenderStateSprite3D();
void MikuPan_SetRenderStateShadow();
void MikuPan_SetRenderStateShadowReceiver();
void MikuPan_SetTriangleStripRestart();
void MikuPan_ResetRenderStateCache(void);
int  MikuPan_GetCurrentRenderStateMode(void);
void MikuPan_ApplyRenderStateMode(int mode);
void MikuPan_BindVAO(u_int vao);
void MikuPan_BindBufferCached(u_int target, u_int buffer);
void MikuPan_ActiveTextureCached(u_int unit);
void MikuPan_BindTexture2DCached(u_int texture);
void MikuPan_SetViewportCached(int x, int y, int w, int h);
void MikuPan_ResetGLBindCache(void);
u_int MikuPan_GetBoundVAO(void);
u_int MikuPan_GetBoundTexture2D(void);

#endif//MIKUPAN_MIKUPAN_PIPELINE_H
