#ifndef MIKUPAN_MIKUPAN_PIPELINE_H
#define MIKUPAN_MIKUPAN_PIPELINE_H

#include "mikupan/mikupan_basictypes.h"

void MikuPan_InitPipeline();
void MikuPan_CreateBufferObjectsInfo(MikuPan_PipelineInfo* pipeline_info, u_int num_buffers);
void MikuPan_SetBufferObjectInfo(MikuPan_BufferObjectInfo* object_info,  int buffer_length,  u_int num_attributes);
void MikuPan_SetBufferAttributeInfo(MikuPan_AttributeInfo* attribute_info, int size, u_int index, int stride, u_int offset);
MikuPan_PipelineInfo* MikuPan_GetPipelineInfo(enum MikuPan_PipelineType pipeline_type);

#endif//MIKUPAN_MIKUPAN_PIPELINE_H
