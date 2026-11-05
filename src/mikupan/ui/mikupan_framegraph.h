#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_MIKUPAN_FRAMEGRAPH_H
#define MIKUPAN_MIKUPAN_FRAMEGRAPH_H

#define FRAME_GRAPH_CAPACITY 600


MIKUPAN_NAMESPACE_BEGIN

typedef struct
{
    float times[FRAME_GRAPH_CAPACITY];///< total wall-clock per frame (ms)
    float cpu[FRAME_GRAPH_CAPACITY];  ///< CPU command-submission time (ms)
    float gpu
        [FRAME_GRAPH_CAPACITY];///< SDL_GPU idle/present wait after submit (ms)
    int count;
    int max_samples;
    float ms_scale;
} FrameTimeGraph;

void FrameTimeGraph_Update(FrameTimeGraph* g, float total_ms, float cpu_ms, float gpu_ms);
int PerfTableBegin(const char* id);
void PerfRow(const char* label, float ms, float total);
void FrameTimeGraph_Draw(FrameTimeGraph* g);


MIKUPAN_NAMESPACE_END
#endif// MIKUPAN_MIKUPAN_FRAMEGRAPH_H
