#ifndef MIKUPAN_BAKE_H
#define MIKUPAN_BAKE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

/// Offline texture baker: decodes every model texture the way the game's GS
/// emulation did, stores the images in a pack (mikupan_bake_format.h) and
/// rewrites the archive entries so each mesh names its texture by id. Every
/// TIM2 picture of the 2D files gets a 2D texture id, stamped into its header
/// when its file loads; the few a TEX0 cannot show from the picture data
/// alone are stored as pixels.
///
/// The game runs it on first launch; tools/sgdstitch exposes it as
/// `sgdstitch bake`. Dependency-free C++17: no SDL, no logging library.

namespace MikuPanBake
{

/// The data set to bake.
struct Source
{
    /// IMG_HD.BIN: { u32 start sector, u32 size } per file.
    std::vector<uint8_t> hd;
    /// Size of IMG_BD.BIN in bytes.
    uint64_t bd_size = 0;
    /// One name per IMG_HD.BIN entry (src/os/eeiop/cdvd/fnameee.h). Names
    /// select the 3D files, .mdl characters and the player costume files;
    /// 2D files are looked for everywhere else.
    std::vector<std::string> names;
    /// Reads `size` bytes at `offset` of IMG_BD.BIN.
    std::function<bool(uint64_t offset, void* buffer, size_t size)> read_bd;
};

/// Where the pack is written. Reads only ever touch bytes written earlier
/// (to confirm that two images are identical, and for the verify pass).
struct Sink
{
    std::function<bool(uint64_t offset, const void* data, size_t size)> write;
    std::function<bool(uint64_t offset, void* buffer, size_t size)> read;
};

struct Options
{
    /// Identifies this bake; stored in the pack header and in every texture
    /// list. Any value that changes between bakes (e.g. the time).
    uint64_t build_stamp = 0;
    /// Re-read every rewritten entry and image from the sink and compare each
    /// mesh, for every frame, costume and black & white variant, with a fresh
    /// decode of the original data; and each stamped 2D picture and derived
    /// texture with the GS decode of its file.
    bool verify = true;
    /// Called while baking: stage name, items done, items total.
    std::function<void(const char* stage, size_t done, size_t total)> progress;
    /// Informational lines (per-file notes, warnings).
    std::function<void(const std::string& line)> log;
    /// Polled between files; returning true stops the bake with an error.
    std::function<bool()> cancelled;
};

struct Stats
{
    size_t files_rewritten = 0;
    size_t sgds = 0;
    size_t slots = 0;
    size_t slots_no_texture = 0;///< TEX0 the game cannot decode
    size_t slots_black_white = 0;
    size_t slots_animated = 0;
    size_t slots_costume = 0;
    size_t textures = 0;///< directory entries (ids)
    size_t images = 0;  ///< distinct images stored
    uint64_t image_bytes = 0;
    uint64_t entry_bytes = 0;     ///< rewritten archive entries in the pack
    size_t uncovered_texels = 0;  ///< texels read from VRAM no upload wrote
    size_t uncovered_textures = 0;
    size_t verified_slots = 0;
    size_t files_2d = 0;         ///< TIM2 files and pk2s of TIM2 files
    size_t pictures_2d = 0;      ///< pictures the game can draw
    size_t pictures_2d_base = 0; ///< CLUT-only, shown with another's image
    size_t pictures_2d_baked = 0;///< stored as pixels
    size_t derived_2d = 0;       ///< kDerivedTextures baked
    uint64_t image_bytes_2d = 0;
    size_t verified_2d = 0;      ///< pictures and derived textures checked
    uint64_t pack_size = 0;
};

/// Bakes `source` into `sink`. On failure returns false and describes the
/// problem in `error`; the sink then holds no valid pack (the header is
/// written last).
bool BakeTextures(const Source& source, const Options& options, Sink* sink,
                  Stats* stats, std::string* error);

}// namespace MikuPanBake

#endif// MIKUPAN_BAKE_H
