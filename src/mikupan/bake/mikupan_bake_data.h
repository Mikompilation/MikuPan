#ifndef MIKUPAN_BAKE_DATA_H
#define MIKUPAN_BAKE_DATA_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/// Readers for the Fatal Frame data the texture baker works on: paks, SGD
/// block chains, mesh texture slots and the three kinds of GS texture upload
/// (TRI2 packets, texture animation frames, TIM2 files). Each parser mirrors
/// what the game reads (function names in the comments) and checks bounds,
/// because the in-game bake runs on whatever data the user has.
///
/// Offsets are absolute offsets into the buffer that was passed in.

namespace MikuPanBake
{

/// Little-endian access, byte-wise so unaligned offsets are fine.
inline uint32_t Rd32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
           | ((uint32_t)p[3] << 24);
}

inline uint16_t Rd16(const uint8_t* p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

inline uint64_t Rd64(const uint8_t* p)
{
    return (uint64_t)Rd32(p) | ((uint64_t)Rd32(p + 4) << 32);
}

inline void Wr32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

inline void Wr16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

inline void Wr64(uint8_t* p, uint64_t v)
{
    Wr32(p, (uint32_t)v);
    Wr32(p + 4, (uint32_t)(v >> 32));
}

/// --- Paks ---------------------------------------------------------------------

/// Pak entry types inside a character .mdl.
constexpr uint32_t kPakTextureAnimation = 4;///< pak of TIM2 paks (unused)
constexpr uint32_t kPakModels = 5;          ///< .mpk: pak of SGDs
constexpr uint32_t kPakTextures = 6;        ///< pak of TIM2 files

struct PakEntry
{
    size_t off = 0;///< entry data
    size_t size = 0;
    uint32_t type = 0;
};

/// A pak is [u32 count][pad x3] followed by `count` entries of
/// [u32 size][u32 type][pad x2][data] (GetFileInPak()). Fails unless the
/// header and every entry are well formed and inside [base, base + size).
/// A count of 0 is rejected: original data never has it.
bool ReadPak(const uint8_t* data, size_t base, size_t size,
             std::vector<PakEntry>* entries);

/// --- SGD ----------------------------------------------------------------------

constexpr uint32_t kSgdVersion = 0x1050;

/// Procedure unit categories (prim[1]).
constexpr uint32_t kUnitVuvn = 0;
constexpr uint32_t kUnitMesh = 1;
constexpr uint32_t kUnitTri2 = 10;
constexpr uint32_t kUnitTextureAnimation = 12;
constexpr uint32_t kUnitMonoClut = 13;

struct SgdRef
{
    size_t top = 0;
    size_t size = 0;
};

/// True if [top, top + size) starts with an SGD HeaderSection.
bool IsSgd(const uint8_t* data, size_t top, size_t size);

/// Every SGD in [base, base + size): the range itself, or the SGDs of
/// (nested) paks, in file order.
void FindSgds(const uint8_t* data, size_t base, size_t size,
              std::vector<SgdRef>* out, int depth = 0);

struct Unit
{
    size_t off = 0;///< absolute
    uint32_t category = 0;
    uint32_t len = 0;
};

/// One chain per HeaderSection::primitives[] entry (GetTopProcUnitHeaderPtr()
/// / GetNextProcUnitHeaderPtr()); a missing chain is an empty vector.
bool WalkChains(const uint8_t* data, const SgdRef& sgd,
                std::vector<std::vector<Unit>>* blocks, std::string* error);

/// Offset of the 16 data bytes of the A+D packet that sets TEX0_1, or 0 if
/// the mesh binds no texture of its own. Mirrors the renderer
/// (mikupan_mesh_render.cpp): 0x02/0x0A/0x82 always read unit + 0x28;
/// 0x12/0x32 read unit + 0x38 when it lies before the colour data, which is
/// exactly when the unit carries the packet (STCYCL 4,4 at unit + 0x20).
/// `marker` receives slot word 2 (6 = TEX0_1, kSlotMarker = baked).
size_t MeshTextureSlot(const uint8_t* data, const Unit& mesh,
                       uint32_t* marker);

/// --- GS uploads ---------------------------------------------------------------

/// One GS image transfer, as GS::GSHelper::Upload() stores it.
struct Upload
{
    int dpsm = 0;
    int dbp = 0;
    int dbw = 0;
    int dsax = 0;
    int dsay = 0;
    int rrw = 0;
    int rrh = 0;
    const uint8_t* data = nullptr;
    size_t available = 0;///< bytes from `data` to the end of the buffer
};

/// Bytes GS::GSHelper::Upload() reads for `u`, 0 for a DPSM it ignores.
size_t UploadBytes(const Upload& u);

/// Uploads of a TRI2 (10) or mono CLUT (13) unit, in the order
/// LoadTRI2Files() sends its packets.
bool ParseTri2Unit(const uint8_t* data, size_t size, const Unit& unit,
                   std::vector<Upload>* out, std::string* error);

/// furn_types.h TextureAnimation: the parameters at prim[2].
struct TextureAnimationInfo
{
    int tex_num = 0;
    int pads = 0;
    size_t frames_off = 0;///< absolute offset of the first frame packet
};

/// A texture animation unit (12): parameters and the uploads of each frame
/// packet (LoadTextureAnimation() mirrors frames 0..current).
bool ParseTextureAnimation(const uint8_t* data, size_t size, const Unit& unit,
                           TextureAnimationInfo* info,
                           std::vector<std::vector<Upload>>* frames,
                           std::string* error);

/// Uploads of the pak of TIM2 files at [base, base + size) of a buffer of
/// `data_size` bytes, placed at a VRAM block offset (SetManmdlTm2() ->
/// MakeTim2Direct3() / MakeClutDirect3()): each file's first picture, its
/// image at TBP0 + offset, then its CLUT at CBP + offset.
bool ParseTim2Pak(const uint8_t* data, size_t data_size, size_t base,
                  size_t size, int offset, std::vector<Upload>* out,
                  std::string* error);

}// namespace MikuPanBake

#endif// MIKUPAN_BAKE_DATA_H
