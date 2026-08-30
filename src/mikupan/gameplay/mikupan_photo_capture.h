#ifndef MIKUPAN_PHOTO_CAPTURE_H
#define MIKUPAN_PHOTO_CAPTURE_H

#include "typedefs.h"

/* MikuPan compatibility hooks for the native GPU photo-capture path.
 * These keep original photo controller/draw code from knowing about renderer
 * history, capture suppression, or staged preview/save internals.
 */
void MikuPan_StagePhotoCaptureForSave(void);
void MikuPan_OnHiddenFurniturePhotoStart(void);

int MikuPan_CapturePhotoForLater(void);
int MikuPan_HasPhotoCapture(void);
void MikuPan_TakePhotoFromResolvedScreen(void);
/* Writes the most recently captured album photo (the same pixels just
 * committed to the in-game album save data) to a PNG in the folder the
 * binary writes to (see MikuPan_ResolveUserPath), named after `slot_no`
 * (the album slot index, i.e. photo_wrk.adr_no). No-op if there is no
 * valid capture staged. */
void MikuPan_ExportAlbumPhotoPng(int slot_no);
void MikuPan_QueueResolvedPhotoPreview(void);
void MikuPan_ActivatePhotoPreviewWithAlpha(u_char alpha);
void MikuPan_ActivatePhotoFramePreview(void);
void MikuPan_ActivatePhotoFrameOverlays(void);
void MikuPan_ClearPhotoFrameOverlay(void);
void MikuPan_ResetPhotoFrameOverlay(void);
void MikuPan_RenderPhotoPreviewInFrame(void);
void MikuPan_RenderPhotoNegativeInFrame(void);
int MikuPan_UsePhotoFrameOverlay(void);
void MikuPan_ActivatePhotoNegativeFromSetNegaFilter(int type, float y12,
                                                    float y22);

#endif// MIKUPAN_PHOTO_CAPTURE_H
