#ifndef MIKUPAN_POINT_DEPTH_H
#define MIKUPAN_POINT_DEPTH_H

#include "graphics/graph2d/effect_sub.h"
#include "mikupan/rendering/mikupan_gpu.h"

/* SDL GPU equivalent of the original PS2 depth visibility test.
 * Uses the same PP_JUDGE payload as CheckPointDepth(), but queries
 * MikuPan's native GPU depth state instead of reading the emulated GS
 * z-buffer through sceGsExecStoreImage(), which may be stale after
 * SDL GPU rendering.
 */

static inline void MikuPan_CheckPointDepth(PP_JUDGE *ppj)
{
    int i;

    if (ppj == NULL)
    {
        return;
    }

    for (i = 0; i < ppj->num; i++)
    {
        int visible = MikuPan_GPUDepthQueryPointVisibleWorld(ppj->p[i]);
        if (visible < 0)
        {
            CheckPointDepth(ppj);
            return;
        }

        ppj->result[i] = visible != 0;
    }
}

#endif // MIKUPAN_POINT_DEPTH_H
