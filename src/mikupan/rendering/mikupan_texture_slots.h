#ifndef MIKUPAN_TEXTURE_SLOTS_H
#define MIKUPAN_TEXTURE_SLOTS_H

#include <stdint.h>

/// 2D textures without GS memory.
///
/// The game names a 2D texture by the TEX0 of the VRAM position it uploaded
/// it to. Instead of VRAM, MikuPan keeps tables indexed by the 256-byte
/// block a TEX0 field names:
///   - the owner of the last upload that starts at each block;
///   - the texture registered for the TEX0s keyed by a block: the CLUT
///     block (CBP) of indexed formats, the texel block (TBP0) otherwise.
/// Uploading a TIM2 picture registers the 2D texture id the texture bake
/// stamped into its header (MikuPanBake::Tim2Stamp). Drawing a TEX0 is one
/// array index and two comparisons: the registered TEX0, and the owner of
/// the texel block the texture was registered with. A texture is decoded
/// once, when first drawn, from a copy of the TIM2 data taken at upload
/// (mikupan_tim2.h), or read from the texture pack. Runtime images (screen
/// copies, the camera photo, the dither noise, photos from save data) are
/// kept as plain pixels in the same tables.

#ifdef __cplusplus
extern "C" {
#endif

/// A TIM2 picture upload: MakeTim2Direct3(), Tim2LoadImage2() and the other
/// TIM2 senders. `picture_header` is the picture in memory, `block` where
/// the game puts its texels (TBP0 plus the caller's offset).
void MikuPan_TexSlots_UploadImage(const void *picture_header, int block);
/// Its CLUT upload: MakeClutDirect3(), Tim2LoadClut2(), ... at CBP plus the
/// caller's offset.
void MikuPan_TexSlots_UploadClut(const void *picture_header, int block);

/// sceGsExecLoadImage(): a runtime image transfer of w x h pixels to (x, y)
/// of a buffer `dbw` * 64 pixels wide at block `dbp`.
void MikuPan_TexSlots_UploadRaw(int dbp, int dbw, int dpsm, int x, int y,
                                int w, int h, const void *data);

/// sceGsExecStoreImage(): reads w x h pixels at (x, y) of a runtime image
/// back. Pixels no runtime image holds read as 0.
void MikuPan_TexSlots_StoreRaw(int sbp, int sbw, int spsm, int x, int y,
                               int w, int h, void *out);

/// The GPU texture ((1 << TW) x (1 << TH) RGBA8) TEX0 `tex0` shows, or 0
/// when nothing registered shows it.
unsigned int MikuPan_TexSlots_Resolve(uint64_t tex0);

/// What MikuPan_TexSlots_Resolve() shows, as a new GPU texture the caller
/// owns (fonts, the map), or 0.
unsigned int MikuPan_TexSlots_CreateCopy(uint64_t tex0);

/// Per-frame counters for the frame graph: TIM2 pictures registered, runtime
/// images uploaded (and their bytes), textures created and the time it
/// took, TEX0s that showed nothing.
void MikuPan_TexSlots_ResetFrameStats(void);
int MikuPan_TexSlots_GetPictureUploads(void);
int MikuPan_TexSlots_GetRawUploads(void);
int MikuPan_TexSlots_GetRawUploadBytes(void);
int MikuPan_TexSlots_GetTexturesCreated(void);
float MikuPan_TexSlots_GetCreateMs(void);
int MikuPan_TexSlots_GetMisses(void);

/// Debug texture list: every GPU texture the slots hold. `id` is the 2D
/// texture id, or 0xFFFFFFFF for a runtime image.
void MikuPan_TexSlots_ForEachTexture(
    void (*callback)(unsigned int texture, int width, int height,
                     uint32_t id, void *userdata),
    void *userdata);

/// Frees the GPU textures; each is created again when next drawn (from the
/// pictures still in memory, the pack, or the kept runtime pixels).
void MikuPan_TexSlots_ReleaseTextures(void);

/// Releases every texture the slots created and forgets every upload.
void MikuPan_TexSlots_Shutdown(void);

#ifdef __cplusplus
}
#endif

#endif// MIKUPAN_TEXTURE_SLOTS_H
