#ifndef MIKUPAN_POINT_DEPTH_H
#define MIKUPAN_POINT_DEPTH_H

#include "graphics/graph2d/effect_sub.h"
#include "mikupan/rendering/mikupan_gpu.h"

static inline void MikuPan_CheckPointDepth(PP_JUDGE *ppj)
{
    int i;

    if (ppj == NULL)
    {
        return;
    }

    for (i = 0; i < ppj->num; i++)
    {
        float tx;
        float ty;
        int visible;

        GetCamI2DPos(ppj->p[i], &tx, &ty);
        visible = MikuPan_GPUDepthQueryPointVisibleWorldScreen(ppj->p[i], tx, ty);
        if (visible < 0)
        {
            CheckPointDepth(ppj);
            return;
        }

        ppj->result[i] = visible != 0;
    }
}

#endif // MIKUPAN_POINT_DEPTH_H
