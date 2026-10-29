#ifndef SGDSTITCH_SGD_STITCH_H
#define SGDSTITCH_SGD_STITCH_H

#include "sgd_units.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace sgd
{

struct StitchOptions
{
    /// Upper bound on strips per stitched mesh (never above 127, see
    /// kMaxStripsPerMesh).
    int max_strips = kMaxStripsPerMesh;
    /// Upper bound on vertices per stitched referenced (skinned) batch.
    int max_referenced_verts = 480;
    /// Upper bound on vertices per stitched inline / map batch.
    int max_inline_verts = 4096;
    /// Re-simulate every rewritten SGD and roll back on any difference.
    bool verify = true;
};

struct StitchStats
{
    uint64_t sgds = 0;         ///< SGDs found
    uint64_t sgds_invalid = 0; ///< failed validation, left untouched
    uint64_t sgds_modified = 0;
    uint64_t sgds_rolled_back = 0;///< verifier mismatch, restored

    uint64_t draws_before = 0;///< main colour pass MESH units
    uint64_t draws_after = 0;
    uint64_t shadow_before = 0;///< shadow-caster-only MESH units
    uint64_t shadow_after = 0;

    uint64_t pairs = 0;            ///< VUVN+MESH pairs examined
    uint64_t pairs_merged_away = 0;///< pairs folded into a previous one
    uint64_t vtype3_batches = 0;   ///< stitched vtype 2 batches emitted as 3
    uint64_t bytes_freed = 0;

    std::map<std::string, uint64_t> unsupported;///< pair layout rejections
    std::map<std::string, uint64_t> breaks;     ///< why adjacent pairs split
    std::map<std::string, uint64_t> locked;     ///< blocks not rewritten

    void Add(const StitchStats& o);
};

/// Stitches one SGD in place. `sgd` points at the HeaderSection and `size` is
/// the number of bytes that belong to it. The SGD keeps its size and every
/// offset outside the rewritten block chains. Returns true if modified.
bool StitchSgd(uint8_t* sgd, size_t size, const StitchOptions& options,
               StitchStats* stats, std::string* log);

}// namespace sgd

#endif// SGDSTITCH_SGD_STITCH_H
