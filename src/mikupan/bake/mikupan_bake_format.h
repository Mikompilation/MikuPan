#ifndef MIKUPAN_BAKE_FORMAT_H
#define MIKUPAN_BAKE_FORMAT_H

#include <cstdint>

/// IMG_TEX.BIN, the baked texture pack, and the baked form of the data that
/// refers to it. Written by the texture baker (mikupan_bake.cpp), read by
/// the game. Everything is little endian.
///
/// The pack holds:
///   - every model texture, decoded once to RGBA8 with the PS2 alpha already
///     expanded (what MikuPan_GPUCreateTextureRGBA8() takes);
///   - a copy of every archive entry the baker rewrote, same size as the
///     original. The game reads those entries from the pack instead of
///     IMG_BD.BIN, so the user's data files are never modified;
///   - the 2D textures: a directory of every TIM2 picture of the 2D files,
///     the 16-byte stamps that name each picture's id inside its header
///     (applied to the entry when the game loads it), and the RGBA8 images
///     of the few textures that cannot be decoded from one picture;
///   - the IMG_HD.BIN the pack was baked from, and 16 bytes from the middle
///     of every source entry it rewrote, to detect a changed data set (an
///     original and a stitched IMG_BD.BIN have the same IMG_HD.BIN and
///     size).
///
/// A baked mesh names its texture with an id that indexes the texture
/// directory, and a 2D picture names its own. Finding a texture is one array
/// index: no hashing, no search.

namespace MikuPanBake
{

constexpr char kPackMagic[4] = {'M', 'P', 'T', 'X'};
/// 1: model textures. 4: also the 2D textures (2 and 3 were never
/// released).
constexpr uint32_t kPackVersion = 4;

/// File name of the pack, next to the user's configuration.
constexpr const char* kPackFileName = "IMG_TEX.BIN";

/// --- Mesh texture slot ------------------------------------------------------
///
/// Every textured mesh carries one GIF A+D packet that sets TEX0_1. Its 16
/// data bytes are at mesh unit + 0x28 (mesh types 0x02/0x0A/0x82) or + 0x38
/// (preset 0x12/0x32 units that carry the packet). Baked, they hold
/// { u32 id, u32 info, u32 kSlotMarker, u32 0 } instead of
/// { TEX0 lo, TEX0 hi, 6 (TEX0_1), 0 }.

/// Slot word 2. 6 (the TEX0_1 register) means "not baked"; SgdAddTexOffset()
/// only relocates slots whose word is 6, so baked ids are never touched.
constexpr uint32_t kSlotMarker = 0x7F;

/// Slot id: bind nothing, keep the current binding (what an undecodable TEX0
/// does today).
constexpr uint32_t kNoTexture = 0xFFFFFFFFu;

/// Slot info bits.
/// Animation frames (0 = static). Frame f binds id + min(f, frames - 1).
constexpr uint32_t kInfoFramesMask = 0xFFFFu;
/// Gray the texel in black & white mode (the disc's BW CLUT for this
/// texture is gray(colour)).
constexpr uint32_t kInfoBlackWhite = 1u << 16;
/// Add the loaded player costume (0-3) to the id.
constexpr uint32_t kInfoCostume = 1u << 17;
/// Black & white follows the player CLUT state (SetPlyrClut) instead of the
/// map black & white state (loadbw_flg).
constexpr uint32_t kInfoPlayerClut = 1u << 18;

/// --- Procedure units --------------------------------------------------------

/// SGD block 0: the first TRI2 unit (category 10) of a baked SGD becomes a
/// texture list unit; the TextureListHeader starts at unit + 16.
constexpr uint32_t kUnitTextureList = 0x50;
/// Every other upload unit (TRI2 10, mono CLUT 13) of a baked SGD. No walker
/// handles it; its payload is zeroed.
constexpr uint32_t kUnitRetired = 0x51;

/// --- Pack layout --------------------------------------------------------------

struct PackHeader
{
    char magic[4];                    ///< kPackMagic
    uint32_t version;                 ///< kPackVersion
    uint64_t build_stamp;             ///< also in every TextureListHeader
    uint32_t texture_count;           ///< texture ids are 0..texture_count-1
    uint32_t entry_count;             ///< IMG_HD.BIN entry count
    uint64_t texture_directory_offset;///< TextureEntry[texture_count]
    uint64_t entry_directory_offset;  ///< EntryOverride[entry_count]
    uint64_t source_hd_offset;        ///< copy of the source IMG_HD.BIN
    uint32_t source_hd_size;          ///< its size in bytes
    uint32_t flags;                   ///< 0
    uint64_t source_bd_size;          ///< size of the source IMG_BD.BIN
    uint64_t pack_size;               ///< size of this file
    uint64_t probe_offset;            ///< EntryProbe[entry_count]
    uint64_t tex2d_directory_offset;  ///< Tex2DEntry[tex2d_count]
    uint32_t tex2d_count;             ///< 2D texture ids are 0..tex2d_count-1
    uint32_t derived_count;
    uint64_t derived_offset;          ///< Tex2DDerived[derived_count]
    uint64_t patch_offset;            ///< PatchRecord[patch_count]
    uint64_t patch_range_offset;      ///< PatchRange[entry_count]
    uint32_t patch_count;
    uint32_t reserved;
};
static_assert(sizeof(PackHeader) == 128, "PackHeader layout");

/// directory[id]. Several ids can share one image: animation frames and
/// costume variants need consecutive ids even when two of them are equal.
struct TextureEntry
{
    uint64_t offset;///< pixels of directory[owner]: width * height RGBA8,
                    ///< rows top-down
    uint16_t width;
    uint16_t height;
    uint32_t owner;///< id whose image this id shows (== id if unique)
};
static_assert(sizeof(TextureEntry) == 16, "TextureEntry layout");

/// entries[file id] of IMG_HD.BIN.
struct EntryOverride
{
    uint64_t offset;///< rewritten entry inside the pack
    uint32_t size;  ///< 0 = read IMG_BD.BIN; else the IMG_HD.BIN size
    uint32_t reserved;
};
static_assert(sizeof(EntryOverride) == 16, "EntryOverride layout");

/// probes[file id]: for an entry with an EntryOverride, the 16 bytes of the
/// source IMG_BD.BIN entry at ProbeOffset(size); zero otherwise.
struct EntryProbe
{
    uint8_t bytes[16];
};
static_assert(sizeof(EntryProbe) == 16, "EntryProbe layout");

/// Where an entry of `size` bytes is probed: its middle, 16-byte aligned.
inline uint32_t ProbeOffset(uint32_t size)
{
    return size < 32 ? 0 : (size / 2) & ~15u;
}

/// What replaces a texture upload container, at the same position and in
/// the same space: every id the container's meshes can bind, so the game can
/// create the GPU textures when the model loads. 16-byte aligned.
///
///   SGD block 0, first TRI2 unit  -> category kUnitTextureList, at unit + 16
///   texture animation unit (12)   -> parameters kept, at the old frame data
///   character .mdl type-6 entry   -> pak header with count 0, at entry + 16
///   player costume .pk2           -> pak header with count 0, at file + 16
struct TextureListHeader
{
    char magic[4];       ///< kPackMagic
    uint32_t count;      ///< ids that follow
    uint64_t build_stamp;///< PackHeader::build_stamp
    uint32_t costume;    ///< player costume .pk2: its costume index, else 0
    uint32_t flags;      ///< kListFlagPlayer for player costume .pk2 files
    uint32_t reserved[2];
    // uint32_t ids[count];
};
static_assert(sizeof(TextureListHeader) == 32, "TextureListHeader layout");

/// TextureListHeader::flags: the list belongs to a player costume. Loading it
/// selects the costume and resets the player CLUT state to colour, as the
/// colour CLUTs inside the costume's TIM2 files did.
constexpr uint32_t kListFlagPlayer = 1u << 0;

/// --- 2D textures ------------------------------------------------------------
///
/// The 2D files (menus, HUD, fonts, effects, item and photo pictures) are
/// TIM2 files or pk2s of TIM2 files. The game uploads each picture at the
/// VRAM position its GsTex0 names, plus an offset some callers add, and
/// draws it by the TEX0 of that position. Every picture gets a 2D texture
/// id, written into its own header where the game never reads: the 16 bytes
/// at picture header + kTim2StampOffset (GsTex1, GsTexaFbaPabe, GsTexClut;
/// 0x260, 0, 0 on every disc picture) hold a Tim2Stamp.
///
/// The game keeps, per 256-byte VRAM block, the id of the last picture whose
/// image or CLUT upload starts there, and the texture registered for the
/// TEX0 at that position: no VRAM, one array index per draw.

constexpr uint32_t kTim2StampOffset = 0x20;
/// Tim2Stamp::marker, "M2DT". The game keeps some files in memory it reuses
/// for other data once they are uploaded, so the stamp must not be easy to
/// meet by chance.
constexpr uint32_t kTim2StampMarker = 0x5444324Du;
/// Tim2Stamp::owner when no upload of the picture's own file starts at its
/// TBP0: the texture cannot be told apart from what other files left there.
constexpr uint32_t kNoOwner = 0xFFFFFFFFu;

struct Tim2Stamp
{
    uint32_t id;    ///< Tex2DEntry this picture's TEX0 shows (kNoTexture:
                    ///< a TEX0 the game cannot draw)
    uint32_t owner; ///< id of the picture whose upload starts at TBP0 when
                    ///< it shows: the picture itself, or the one whose image
                    ///< a CLUT-only picture shows with its own CLUT
    uint32_t marker;///< kTim2StampMarker
    int32_t base;   ///< from this header to the header of the picture whose
                    ///< texels the texture shows (0: its own)
};
static_assert(sizeof(Tim2Stamp) == 16, "Tim2Stamp layout");

/// tex2d[id].
struct Tex2DEntry
{
    uint64_t offset;       ///< RGBA8 pixels, width x height, rows top-down;
                           ///< 0: decoded from the pictures as they are
                           ///< uploaded (DecodeTim2Texture(), mikupan_tim2.h)
    uint64_t tex0;         ///< the TEX0 that shows it, at offset 0. The game
                           ///< rewrites parts of GsTex0 in memory, so the
                           ///< texture is registered by this value.
    uint16_t width;        ///< 1 << TW
    uint16_t height;       ///< 1 << TH
    uint32_t derived_first;///< Tex2DDerived records this picture's upload
    uint32_t derived_count;///< completes
    uint32_t reserved;
};
static_assert(sizeof(Tex2DEntry) == 32, "Tex2DEntry layout");

/// A texture the game draws by a TEX0 that is no picture's own: its texels
/// or CLUT lie inside other pictures of one file (the message box and the
/// button icons inside the effect pack, a number font inside the camera
/// HUD). Registered at the offset of the upload of the picture that
/// completes it.
struct Tex2DDerived
{
    uint64_t tex0;       ///< what the game draws, at offset 0
    uint32_t id;         ///< its Tex2DEntry (pixels in the pack)
    uint32_t check_block;///< the texture shows while the last upload that
    uint32_t check_owner;///< starts at this block (offset 0) is this id's
    uint32_t reserved;
};
static_assert(sizeof(Tex2DDerived) == 24, "Tex2DDerived layout");

/// patch_ranges[file id]: the PatchRecords applied to that entry.
struct PatchRange
{
    uint32_t first;
    uint32_t count;
};
static_assert(sizeof(PatchRange) == 8, "PatchRange layout");

/// 16 bytes of an archive entry replaced when the game loads it (a
/// Tim2Stamp), only while they still hold `before`.
struct PatchRecord
{
    uint32_t offset;///< inside the entry
    uint32_t reserved;
    uint8_t before[16];
    uint8_t after[16];
};
static_assert(sizeof(PatchRecord) == 40, "PatchRecord layout");

}// namespace MikuPanBake

#endif// MIKUPAN_BAKE_FORMAT_H
