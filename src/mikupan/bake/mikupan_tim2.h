#ifndef MIKUPAN_TIM2_H
#define MIKUPAN_TIM2_H

#include <cstddef>
#include <cstdint>
#include <vector>

/// TIM2 pictures, the 2D textures of the game, and the texture a TEX0 shows
/// once a picture's image and CLUT were the last uploads at the TBP0 and CBP
/// it names, decoded straight from the picture's storage order. No GS
/// memory: the game's 2D texture slots decode with it, and the texture baker
/// checks it against the GS emulation for every picture on the disc.
///
/// Dependency-free C++17, like the rest of the baker library.

namespace MikuPanBake
{

/// The fields of a TIM2 picture header (tim2.h TIM2_PICTUREHEADER) the game
/// uses to upload and draw it.
struct Tim2Picture
{
    const uint8_t* header = nullptr;
    const uint8_t* image = nullptr;///< header + HeaderSize
    const uint8_t* clut = nullptr; ///< image + ImageSize
    uint32_t image_size = 0;
    uint32_t clut_size = 0;
    uint16_t clut_colors = 0;
    uint8_t clut_type = 0;
    uint8_t image_type = 0;
    int width = 0;
    int height = 0;
    uint64_t tex0 = 0;///< GsTex0
};

/// Bytes of a picture header up to and including GsTexClut.
constexpr size_t kTim2PictureHeaderSize = 0x30;

/// Reads the picture header at `header`, of which `available` bytes are
/// readable. Fails unless the header, the image and the CLUT lie inside.
bool ReadTim2Picture(const uint8_t* header, size_t available,
                     Tim2Picture* out);

/// Absolute offsets of the picture headers of a 2D texture file: a TIM2
/// file (its pictures, walked by TotalSize like Tim2GetPictureHeader()), or
/// a pk2 of TIM2 files - u32 count, 3 pad words, then `count` u32 offsets
/// from the pk2 start (MakeTim2ClutDirect3()). Fails for anything else.
bool FindTim2Pictures(const uint8_t* data, size_t size,
                      std::vector<size_t>* headers);

/// TEX0 fields, as GS::DecodeTex0() splits them.
struct Tim2Tex0
{
    int tbp0 = 0;
    int tbw = 0;
    int psm = 0;
    int tw = 0;
    int th = 0;
    int cbp = 0;
    int csa = 0;
};

Tim2Tex0 SplitTex0(uint64_t tex0);

/// One PSMCT32 texel (or CLUT entry) as RGBA8, PS2 alpha expanded like
/// GS::AdjustPS2Alpha().
void Ct32ToRgba8(const uint8_t* texel, uint8_t* out);

/// One PSMCT16(S) texel as RGBA8: 5:5:5:1, the top bit is alpha 0x80.
void Ct16ToRgba8(uint16_t texel, uint8_t* out);

/// The entry (x, y) PSMT8 index `index` reads in its 16x16 CSM1 CLUT: index
/// bits 3 and 4 swapped.
void T8ClutEntry(int index, int* x, int* y);

/// True when the game binds a texture for `tex0`: what
/// GS::GetRawTextureRegion() and GS::IsDecodableTexture() accept (texel
/// (0, 0) is at TBP0 * 256 for every format, and the texture's bytes from
/// there fit in the 4 MB of GS memory).
bool Tex0Drawable(uint64_t tex0);

/// Writes the (1 << TW) x (1 << TH) RGBA8 texture `tex0` shows after
/// MakeTim2Direct3() uploaded `image`'s texels at TBP0 and
/// MakeClutDirect3() uploaded `clut`'s CLUT at CBP: what
/// GS::GSHelper::DecodeTexture() reads back, PS2 alpha expanded. Texels
/// outside the image are 0. Fails when the result is not that simple: an
/// image laid out differently from `tex0` or wider than its buffer, a CLUT
/// the texture reads outside of, or a format the game cannot decode.
bool DecodeTim2Texture(const Tim2Picture& image, const Tim2Picture& clut,
                       uint64_t tex0, uint8_t* out);

}// namespace MikuPanBake

#endif// MIKUPAN_TIM2_H
