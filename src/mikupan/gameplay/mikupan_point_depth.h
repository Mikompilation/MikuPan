#ifndef MIKUPAN_POINT_DEPTH_H
#define MIKUPAN_POINT_DEPTH_H

#include "graphics/graph2d/effect_sub.h"
#include "mikupan/rendering/mikupan_gpu.h"

#define MIKUPAN_POINT_DEPTH_KEY_DEFAULT 0
#define MIKUPAN_POINT_DEPTH_KEY_ENEMY_IN_DISPLAY 1
#define MIKUPAN_POINT_DEPTH_KEY_ENEMY_FRAME_HIT 2
#define MIKUPAN_POINT_DEPTH_KEY_FURN_FRAME_POINT 3
#define MIKUPAN_POINT_DEPTH_KEY_FURN_HINT 4

static inline void MikuPan_CheckPointDepthKeyed(PP_JUDGE *ppj, int kind,
                                                int object_id,
                                                int point_base)
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
        visible = MikuPan_GPUDepthQueryPointVisibleWorldScreenQueued(
            kind, object_id, point_base + i, ppj->p[i], tx, ty);
        if (visible < 0)
        {
            CheckPointDepth(ppj);
            return;
        }

        ppj->result[i] = visible != 0;
    }
}

static inline void MikuPan_CheckPointDepth(PP_JUDGE *ppj)
{
    MikuPan_CheckPointDepthKeyed(ppj, MIKUPAN_POINT_DEPTH_KEY_DEFAULT, 0, 0);
}

#endif
