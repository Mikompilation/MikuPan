#include "sgd_layout.h"
#include "sgd_io.h"

#include <algorithm>

namespace sgd
{
namespace
{

constexpr size_t kHeaderSize = 0x18;
constexpr uint32_t kMaxBlocks = 4096;
constexpr size_t kMaxUnitsPerChain = 1u << 20;
constexpr size_t kSentinelSize = 16;

/// PHEAD word indices (sg_dat.h).
constexpr int kPheadUniqHeaderSize = 1;
constexpr int kPheadUniqList = 4;
constexpr int kPheadWeightedVertex = 10;
constexpr int kPheadWeightedNormal = 11;
constexpr int kPheadWeightedList = 12;
constexpr int kPheadWords = 13;

bool Fail(std::string* error, const char* why)
{
    if (error != nullptr)
    {
        *error = why;
    }

    return false;
}

/// Sum of a VERTEXLIST's vertex counts exactly as MappingVertexList() does it
/// (it reads ONELIST::vIdx and vOff together as one int).
bool VertexListSize(const uint8_t* sgd, size_t size, size_t off,
                    uint64_t* total, size_t* next)
{
    if (off + 4 > size)
    {
        return false;
    }

    const uint32_t count = Rd32(sgd + off);

    if (count > 65536 || off + 4 + (size_t) count * 8 > size)
    {
        return false;
    }

    uint64_t sum = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        sum += Rd32(sgd + off + 4 + (size_t) i * 8 + 4);
    }

    *total = sum;
    *next = off + 4 + (size_t) count * 8;
    return true;
}

SkinInfo ComputeSkinInfo(const uint8_t* sgd, size_t size, uint32_t phead)
{
    SkinInfo skin;
    const uint8_t* ph = sgd + phead;
    const uint32_t uniq_header_size = Rd32(ph + 4 * kPheadUniqHeaderSize);
    const uint32_t uniq_list = Rd32(ph + 4 * kPheadUniqList);
    const uint32_t weighted_vertex = Rd32(ph + 4 * kPheadWeightedVertex);
    const uint32_t weighted_normal = Rd32(ph + 4 * kPheadWeightedNormal);
    const uint32_t weighted_list = Rd32(ph + 4 * kPheadWeightedList);

    skin.weighted_vertex = weighted_vertex;

    if (weighted_list == 0 || weighted_vertex == 0 || weighted_normal == 0)
    {
        skin.plain_possible = true;
        return skin;
    }

    /// SgMapUnit() only ever drops the weighted list inside the unique-list
    /// branch, and only when one of the lists overflows the VN buffer.
    if (uniq_list == 0 || uniq_header_size != 4)
    {
        skin.plain_possible = false;
        return skin;
    }

    uint64_t positions;
    uint64_t normals;
    size_t next;

    if (!VertexListSize(sgd, size, weighted_list, &positions, &next)
        || !VertexListSize(sgd, size, next, &normals, &next))
    {
        skin.plain_possible = true;
        return skin;
    }

    skin.plain_possible =
        positions > kVnBufferEntries || normals > kVnBufferEntries;
    return skin;
}

bool WalkChain(const uint8_t* sgd, size_t size, size_t start, Block* block,
               std::string* error)
{
    size_t p = start;

    if ((start & 15) != 0 || start < kHeaderSize || start >= size)
    {
        return Fail(error, "chain start out of bounds");
    }

    while (true)
    {
        if (p + kSentinelSize > size)
        {
            return Fail(error, "chain runs past the SGD");
        }

        const uint32_t next = Rd32(sgd + p);

        if (next == 0)
        {
            break;
        }

        if ((next & 15) != 0 || p + next > size)
        {
            return Fail(error, "bad procedure unit length");
        }

        block->units.push_back({p, Rd32(sgd + p + 4), next});
        p += next;

        if (block->units.size() > kMaxUnitsPerChain)
        {
            return Fail(error, "chain too long");
        }
    }

    block->present = true;
    block->start = start;
    block->sentinel = p;
    block->end = p + kSentinelSize;
    return true;
}

}// namespace

bool LooksLikeSgd(const uint8_t* data, size_t size)
{
    if (size < kHeaderSize + 4 || Rd32(data) != kSgdVersion || data[4] != 0)
    {
        return false;
    }

    const uint32_t blocks = Rd32(data + 0x14);
    const uint32_t phead = Rd32(data + 0x10);

    return blocks != 0 && blocks <= kMaxBlocks
           && kHeaderSize + (size_t) blocks * 4 <= size && phead != 0
           && (size_t) phead + kPheadWords * 4 <= size;
}

bool ReadLayout(const uint8_t* sgd, size_t size, SgdLayout* layout,
                std::string* error)
{
    if (!LooksLikeSgd(sgd, size))
    {
        return Fail(error, "not an unmapped SGD");
    }

    SgdLayout out;
    out.blocks = Rd32(sgd + 0x14);
    out.kind = sgd[5];
    out.block.resize(out.blocks);

    const uint32_t phead = Rd32(sgd + 0x10);
    out.skin = ComputeSkinInfo(sgd, size, phead);

    for (uint32_t i = 0; i < out.blocks; i++)
    {
        const uint32_t start = Rd32(sgd + kHeaderSize + i * 4);

        if (start != 0 && !WalkChain(sgd, size, start, &out.block[i], error))
        {
            return false;
        }
    }

    /// Offsets the header and PHEAD point at; a chain must not contain them.
    /// PHEAD holds three (vertex, normal, list) pool triples; an empty pool
    /// has all three equal and just marks the end of the PHEAD area.
    std::vector<uint32_t> pointers = {Rd32(sgd + 0x08), Rd32(sgd + 0x0c),
                                      phead};
    for (int first : {2, 6, 10})
    {
        const uint32_t vtx = Rd32(sgd + phead + 4 * first);
        const uint32_t nrm = Rd32(sgd + phead + 4 * (first + 1));
        const uint32_t lst = Rd32(sgd + phead + 4 * (first + 2));

        if (vtx == nrm && nrm == lst)
        {
            continue;
        }

        pointers.push_back(vtx);
        pointers.push_back(nrm);
        pointers.push_back(lst);
    }

    /// CheckMirrorModel(): a unit of type 0x40 in block 0 names the block
    /// whose first mesh is used as the mirror surface. Leave it alone.
    int mirror_block = -1;
    if (out.blocks > 0)
    {
        for (const Unit& u : out.block[0].units)
        {
            if (u.type == kPuMirror && u.len >= 16)
            {
                mirror_block = (int) Rd32(sgd + u.off + 8);
            }
        }
    }

    for (uint32_t i = 0; i < out.blocks; i++)
    {
        Block& b = out.block[i];

        if (!b.present)
        {
            continue;
        }

        b.rewritable = true;

        if ((int) i == mirror_block)
        {
            b.rewritable = false;
            b.locked_reason = "mirror block";
            continue;
        }

        for (uint32_t ptr : pointers)
        {
            if (ptr != 0 && ptr >= b.start && ptr < b.end)
            {
                b.rewritable = false;
                b.locked_reason = "chain overlaps header data";
            }
        }

        for (uint32_t j = 0; j < out.blocks; j++)
        {
            const Block& o = out.block[j];

            if (j != i && o.present && o.start < b.end && b.start < o.end)
            {
                b.rewritable = false;
                b.locked_reason = "chain overlaps another block";
            }
        }
    }

    *layout = std::move(out);
    return true;
}

void ComputePairBones(const uint8_t* sgd, size_t size, const SkinInfo& skin,
                      Pair* pair)
{
    pair->bones_known = false;
    pair->bones_uniform = false;

    if (pair->kind != VuvnKind::kReferenced
        || (pair->vtype != 2 && pair->vtype != 3) || skin.weighted_vertex == 0)
    {
        return;
    }

    /// Plain skinning reads the record at pWeightedVertex + ref * 32 and takes
    /// the bone pair from bytes 0x1c/0x1d (SkinCalcLoopType2/3).
    bool uniform = true;
    for (int i = 0; i < pair->vnum; i++)
    {
        const uint64_t ref = Rd32(&pair->vertices[(size_t) i * 8]);
        const uint64_t rec = skin.weighted_vertex + ref * 32;

        if (rec + 32 > size)
        {
            return;
        }

        const uint8_t b0 = sgd[rec + 0x1c];
        const uint8_t b1 = sgd[rec + 0x1d];

        if (i == 0)
        {
            pair->first_bone0 = b0;
            pair->first_bone1 = b1;
        }
        else if (b0 != pair->first_bone0 || b1 != pair->first_bone1)
        {
            uniform = false;
        }
    }

    pair->bones_known = true;
    pair->bones_uniform = uniform;
}

}// namespace sgd
