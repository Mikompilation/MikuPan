#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_PHOTO_HINT_H
#define MIKUPAN_PHOTO_HINT_H

#include "mikupan/rendering/mikupan_renderer.h"

/* Hidden-photo hint overlays are drawn while MikuPan also redraws the
 * captured photo preview at frame end, so the original immediate draw order
 * would place the preview over the hint. Queue the hint quad/sprite into the
 * late 2D overlay pass, keeping DrawPhotoHinttex2() geometry otherwise
 * identical to the original game path.
 */

MIKUPAN_NAMESPACE_BEGIN

static inline void MikuPan_BeginPhotoHintOverlayQueue(void)
{
    MikuPan_BeginLate2DOverlayQueue();
}

static inline void MikuPan_EndPhotoHintOverlayQueue(void)
{
    MikuPan_EndLate2DOverlayQueue();
}


MIKUPAN_NAMESPACE_END
#endif /* MIKUPAN_PHOTO_HINT_H */
