#ifndef SGDSTITCH_SGD_LAYOUT_H
#define SGDSTITCH_SGD_LAYOUT_H

#include "sgd_units.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sgd
{

/// One procedure unit of a block chain.
struct Unit
{
    size_t off = 0;///< relative to the SGD top
    uint32_t type = 0;
    uint32_t len = 0;
};

/// A block is the chain hanging off HeaderSection::primitives[i]. Chains are
/// linked by relative `pNext` offsets and end with a 16-byte sentinel unit
/// whose pNext is 0.
struct Block
{
    bool present = false;
    size_t start = 0;   ///< first unit
    size_t sentinel = 0;///< terminating unit
    size_t end = 0;     ///< sentinel + 16
    std::vector<Unit> units;

    /// The chain occupies [start, end) exclusively and may be rewritten in
    /// place. False for the mirror block, overlapping chains, or chains that
    /// share bytes with the header-referenced regions.
    bool rewritable = false;
    const char* locked_reason = "";
};

/// Facts about the PHEAD vertex pools that decide how vtype 2/3 batches are
/// skinned at runtime (sgsu.cpp SetVUVNDataPost, shadow.cpp
/// SetVUVNDataShadowModel).
struct SkinInfo
{
    /// True when the runtime can take the "plain" skinning path where a
    /// vtype 2 batch uses the bone pair of its first vertex for all vertices
    /// (no weighted vertex list, or SgMapUnit drops it).
    bool plain_possible = true;
    /// PHEAD::pWeightedVertex (32-byte records, bone pair at +0x1c/+0x1d).
    uint32_t weighted_vertex = 0;
};

struct SgdLayout
{
    uint32_t blocks = 0;
    uint8_t kind = 0;
    std::vector<Block> block;
    SkinInfo skin;
};

/// Validates the HeaderSection and walks every block chain. Fails (and the
/// SGD must be left untouched) on anything malformed.
bool ReadLayout(const uint8_t* sgd, size_t size, SgdLayout* layout,
                std::string* error);

/// Cheap header sniff used when scanning archives and paks.
bool LooksLikeSgd(const uint8_t* data, size_t size);

/// Fills the bone metadata of a referenced vtype 2/3 pair.
void ComputePairBones(const uint8_t* sgd, size_t size, const SkinInfo& skin,
                      Pair* pair);

}// namespace sgd

#endif// SGDSTITCH_SGD_LAYOUT_H
