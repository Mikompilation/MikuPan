#ifndef SGDSTITCH_SGD_UNITS_H
#define SGDSTITCH_SGD_UNITS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sgd
{

/// SGD files start with this VersionID (HeaderSection::VersionID).
constexpr uint32_t kSgdVersion = 0x1050;

/// Procedure unit categories (prim[1]), see sgsgd.cpp / sgsu.cpp.
constexpr uint32_t kPuVuvn = 0;
constexpr uint32_t kPuMesh = 1;
constexpr uint32_t kPuMaterial = 2;
constexpr uint32_t kPuCoordinate = 3;
constexpr uint32_t kPuBoundingBox = 4;
constexpr uint32_t kPuMirror = 0x40;

/// GET_NUM_MESH() reads the strip count as a plain `char`, which is signed on
/// x86/x64: anything above 127 would make the renderer loops run zero times.
constexpr int kMaxStripsPerMesh = 127;

/// Referenced-vertex batches are expanded into the `objwork` scratch buffer
/// (2 qwords per vertex + header) before drawing. FlushModel() only keeps
/// 0x400 qwords of headroom, so a batch must stay below ~510 vertices.
constexpr int kMaxReferencedVertsHardLimit = 500;

/// gra3d.cpp: SgSetVNBuffer(VNBufferAddress, 4000) -> vnbuf_size = 2000.
/// SgMapUnit() drops the weighted vertex list when it does not fit.
constexpr uint32_t kVnBufferEntries = 2000;

enum class MeshFamily
{
    kTextured,  ///< 0x02 / 0x0A / 0x82: VU1 DRAWTYPE2, ST per vertex
    kUntextured,///< 0x00 / 0x08 / 0x80: VU1 DRAWTYPE0 (shadow casters)
    kPreset,    ///< 0x12 / 0x32: pre-lit map geometry (colour + ST)
};

enum class VuvnKind
{
    kReferenced,   ///< vertex/normal offsets into the PHEAD vertex pools
    kInline,       ///< interleaved position+normal stored in the unit (0x8x)
    kPresetSmooth, ///< 0x12: interleaved position+normal per vertex
    kPresetFlat,   ///< 0x32: one normal per strip, then the positions
};

struct Strip
{
    uint32_t count = 0;         ///< vertices in the triangle strip
    std::vector<uint8_t> st;    ///< count * 8 bytes (S,T floats), textured
    std::vector<uint8_t> colors;///< count * 12 bytes (R,G,B floats), preset
};

/// One VUVN procedure unit plus the MESH unit that consumes it. This is the
/// granularity of a draw call in MikuPan: every MESH unit is one draw.
struct Pair
{
    size_t vuvn_off = 0;///< offsets relative to the SGD top
    size_t vuvn_len = 0;
    size_t mesh_off = 0;
    size_t mesh_len = 0;

    uint8_t mesh_type = 0;
    MeshFamily family = MeshFamily::kTextured;
    VuvnKind kind = VuvnKind::kReferenced;

    /// VUVN unit
    uint8_t vtype = 0;///< pool: 0 unique, 2/3 bone-skinned, 4 weighted
    uint8_t vuvn_b14 = 0;
    uint8_t vuvn_b15 = 0;
    uint32_t w0 = 0;///< first word of the VU1 header qword
    int vnum = 0;
    int nnum = 0;
    std::vector<uint8_t> vertices;///< refs 8B, records 24B or positions 12B
    std::vector<uint8_t> normals; ///< kPresetFlat: one 12-byte normal/strip

    /// MESH unit
    uint8_t mesh_pad0 = 0;
    uint8_t mesh_pad1 = 0;
    bool has_ad = false;///< carries the GIF A+D packet that sets TEX0_1
    uint32_t ad[8] = {};
    uint32_t tag_w1 = 0;///< preset strip GIF tag (PRIM/FLG/NREG)
    uint32_t tag_w2 = 0;///< preset strip GIF tag (REGS)
    std::vector<Strip> strips;

    /// Skinning metadata for referenced vtype 2/3 batches (see sgsu.cpp
    /// SkinCalcLoopType2: vtype 2 skins the whole batch with the bone pair
    /// stored in its first vertex record).
    bool bones_known = false;
    uint8_t first_bone0 = 0;
    uint8_t first_bone1 = 0;
    bool bones_uniform = false;
};

/// Parses a VUVN+MESH pair and checks that the canonical builders below
/// reproduce both units byte for byte. Only pairs that round-trip exactly are
/// ever merged, so every layout the tool rewrites is one it fully understands.
/// `sgd` points at the SGD top; `v`/`m` are unit offsets from it.
bool ParsePair(const uint8_t* sgd, size_t sgd_size, size_t v, size_t m,
               Pair* pair, std::string* reason);

/// Builds the canonical VUVN unit for `pair` (all PS2 VU1 addresses and DMA
/// sizes are recomputed from the content).
void BuildVuvn(const Pair& pair, std::vector<uint8_t>* out);

/// Builds the canonical MESH unit for `pair`.
void BuildMesh(const Pair& pair, std::vector<uint8_t>* out);

/// Number of draw-relevant strips in the pair.
inline int StripCount(const Pair& pair)
{
    return (int) pair.strips.size();
}

/// True for mesh types MikuPan draws in the main colour pass (they are drawn
/// again by the shadow caster or shadow receiver pass).
bool IsMainPassMesh(uint8_t mesh_type);

/// True for untextured mesh types that are only drawn as shadow casters.
bool IsShadowOnlyMesh(uint8_t mesh_type);

}// namespace sgd

#endif// SGDSTITCH_SGD_UNITS_H
