#include "sgd_stitch.h"
#include "sgd_io.h"
#include "sgd_layout.h"
#include "sgd_verify.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace sgd
{

void StitchStats::Add(const StitchStats& o)
{
    sgds += o.sgds;
    sgds_invalid += o.sgds_invalid;
    sgds_modified += o.sgds_modified;
    sgds_rolled_back += o.sgds_rolled_back;
    draws_before += o.draws_before;
    draws_after += o.draws_after;
    shadow_before += o.shadow_before;
    shadow_after += o.shadow_after;
    pairs += o.pairs;
    pairs_merged_away += o.pairs_merged_away;
    vtype3_batches += o.vtype3_batches;
    bytes_freed += o.bytes_freed;

    for (const auto& [k, v] : o.unsupported)
    {
        unsupported[k] += v;
    }

    for (const auto& [k, v] : o.breaks)
    {
        breaks[k] += v;
    }

    for (const auto& [k, v] : o.locked)
    {
        locked[k] += v;
    }
}

namespace
{

/// Consecutive pairs that will become one VUVN+MESH pair (one draw call).
struct Group
{
    std::vector<const Pair*> pairs;
    int material = -1;///< MATERIAL index in effect for the whole group
    int strips = 0;
    int verts = 0;

    bool Empty() const
    {
        return pairs.empty();
    }

    void Start(const Pair* p, int mat)
    {
        pairs.assign(1, p);
        material = mat;
        strips = StripCount(*p);
        verts = p->vnum;
    }

    void Add(const Pair* p)
    {
        pairs.push_back(p);
        strips += StripCount(*p);
        verts += p->vnum;
    }
};

bool IsSkinned(uint8_t vtype)
{
    return vtype == 2 || vtype == 3;
}

bool SameBones(const Pair& a, const Pair& b)
{
    return a.bones_known && b.bones_known && a.first_bone0 == b.first_bone0
           && a.first_bone1 == b.first_bone1;
}

/// vtype of a stitched batch. vtype 2 skins every vertex of a batch with the
/// bone pair of the batch's first vertex, vtype 3 uses each vertex's own
/// record. Batches with different bone pairs can still be stitched when
/// every vertex record already carries its batch's bone pair, by emitting
/// vtype 3 - the result is identical on the CPU, GPU and shadow paths.
uint8_t MergedVtype(const Group& g, const SkinInfo& skin)
{
    const Pair& a = *g.pairs.front();

    if (a.kind != VuvnKind::kReferenced || !IsSkinned(a.vtype))
    {
        return a.vtype;
    }

    bool all2 = true;
    bool same = true;
    for (const Pair* p : g.pairs)
    {
        all2 = all2 && p->vtype == 2;
        same = same && SameBones(a, *p);
    }

    if (!skin.plain_possible)
    {
        /// Weighted-list path: 2 and 3 are handled identically.
        return all2 ? 2 : 3;
    }

    return (all2 && same) ? 2 : 3;
}

/// Returns nullptr when `b` can join the group, otherwise why not.
const char* CanJoin(const Group& g, const Pair& b, const SkinInfo& skin,
                    const StitchOptions& opt)
{
    const Pair& a = *g.pairs.front();

    if (b.mesh_type != a.mesh_type)
    {
        return "different mesh type";
    }

    if (b.vuvn_b14 != a.vuvn_b14 || b.vuvn_b15 != a.vuvn_b15 || b.w0 != a.w0
        || b.mesh_pad0 != a.mesh_pad0 || b.mesh_pad1 != a.mesh_pad1)
    {
        return "different unit header";
    }

    if (a.family == MeshFamily::kPreset
        && (b.tag_w1 != a.tag_w1 || b.tag_w2 != a.tag_w2))
    {
        return "different strip GIF tag";
    }

    /// A unit carrying an A+D packet binds its TEX0. A preset unit without
    /// one keeps the texture bound by the previous unit, which inside a
    /// group is the group's own texture.
    if (b.has_ad && (!a.has_ad || std::memcmp(a.ad, b.ad, sizeof(a.ad)) != 0))
    {
        return "different texture";
    }

    if (g.strips + StripCount(b) > std::min(opt.max_strips, kMaxStripsPerMesh))
    {
        return "strip limit";
    }

    if (a.kind != VuvnKind::kReferenced)
    {
        if (g.verts + b.vnum > std::min(opt.max_inline_verts, 32767))
        {
            return "vertex limit";
        }

        return a.vtype == b.vtype ? nullptr : "different vertex pool";
    }

    if (g.verts + b.vnum
        > std::min(opt.max_referenced_verts, kMaxReferencedVertsHardLimit))
    {
        return "vertex limit";
    }

    if (!IsSkinned(a.vtype) || !IsSkinned(b.vtype))
    {
        return a.vtype == b.vtype ? nullptr : "different vertex pool";
    }

    if (!skin.plain_possible)
    {
        return nullptr;
    }

    bool all2_same = b.vtype == 2 && SameBones(a, b);
    for (const Pair* p : g.pairs)
    {
        all2_same = all2_same && p->vtype == 2 && SameBones(a, *p);
    }

    if (all2_same)
    {
        return nullptr;
    }

    if (b.vtype == 2 && !b.bones_uniform)
    {
        return "batch mixes bone pairs";
    }

    for (const Pair* p : g.pairs)
    {
        if (p->vtype == 2 && !p->bones_uniform)
        {
            return "batch mixes bone pairs";
        }
    }

    return nullptr;
}

Pair Merge(const Group& g, const SkinInfo& skin)
{
    Pair m = *g.pairs.front();

    m.vtype = MergedVtype(g, skin);
    m.vnum = 0;
    m.nnum = 0;
    m.vertices.clear();
    m.normals.clear();
    m.strips.clear();

    for (const Pair* p : g.pairs)
    {
        m.vnum += p->vnum;
        m.nnum += p->nnum;
        m.vertices.insert(m.vertices.end(), p->vertices.begin(),
                          p->vertices.end());
        m.normals.insert(m.normals.end(), p->normals.begin(),
                         p->normals.end());
        m.strips.insert(m.strips.end(), p->strips.begin(), p->strips.end());
    }

    return m;
}

/// Room far-LOD units (position-only VUVN) are mapped but never drawn.
bool IsPositionOnlyVuvn(const uint8_t* unit)
{
    return Rd32(unit) >= 0x2c && Rd32(unit + 0x28) == 0x01000102
           && unit[14] == 5;
}

void CountMeshes(const uint8_t* chain, const std::vector<Unit>& units,
                 uint64_t* main, uint64_t* shadow)
{
    for (size_t i = 0; i < units.size(); i++)
    {
        const Unit& u = units[i];

        if (u.type != kPuMesh)
        {
            continue;
        }

        if (i > 0 && units[i - 1].type == kPuVuvn
            && IsPositionOnlyVuvn(chain + units[i - 1].off))
        {
            continue;
        }

        const uint8_t mt = chain[u.off + 13];

        if (IsMainPassMesh(mt))
        {
            (*main)++;
        }
        else if (IsShadowOnlyMesh(mt))
        {
            (*shadow)++;
        }
    }
}

/// Counts the MESH units of a freshly built chain (offsets relative to it).
void CountBuiltChain(const std::vector<uint8_t>& chain, uint64_t* main,
                     uint64_t* shadow)
{
    std::vector<Unit> units;
    size_t p = 0;

    while (p + 16 <= chain.size() && Rd32(&chain[p]) != 0)
    {
        units.push_back({p, Rd32(&chain[p + 4]), Rd32(&chain[p])});
        p += Rd32(&chain[p]);
    }

    CountMeshes(chain.data(), units, main, shadow);
}

/// Builds the stitched version of one block chain into `out`. Returns true
/// if at least one pair was folded into another.
bool RewriteBlock(const uint8_t* sgd, size_t size, const Block& blk,
                  const SkinInfo& skin, const StitchOptions& opt,
                  StitchStats* stats, std::vector<uint8_t>* out)
{
    enum class Kind
    {
        kRaw,
        kMaterial,
        kPair,
    };

    struct Item
    {
        Kind kind;
        size_t off;
        uint32_t len;
        int material;
        size_t pair;
    };

    std::vector<Pair> pairs;
    std::vector<Item> items;
    const size_t n = blk.units.size();

    for (size_t i = 0; i < n;)
    {
        const Unit& u = blk.units[i];

        if (u.type == kPuVuvn && i + 1 < n && blk.units[i + 1].type == kPuMesh)
        {
            const Unit& mesh = blk.units[i + 1];
            Pair pair;
            std::string why;

            stats->pairs++;

            if (ParsePair(sgd, size, u.off, mesh.off, &pair, &why))
            {
                ComputePairBones(sgd, size, skin, &pair);
                pairs.push_back(std::move(pair));
                items.push_back({Kind::kPair, u.off, 0, -1, pairs.size() - 1});
            }
            else
            {
                stats->unsupported[why]++;
                items.push_back({Kind::kRaw, u.off, u.len, -1, 0});
                items.push_back({Kind::kRaw, mesh.off, mesh.len, -1, 0});
            }

            i += 2;
            continue;
        }

        if (u.type == kPuMaterial && u.len >= 16)
        {
            items.push_back({Kind::kMaterial, u.off, u.len,
                             (int) Rd32(sgd + u.off + 8), 0});
        }
        else
        {
            items.push_back({Kind::kRaw, u.off, u.len, -1, 0});
        }

        i++;
    }

    std::vector<uint8_t>& o = *out;
    std::vector<uint8_t> built;
    std::vector<const Item*> pending;///< same-material MATERIAL units
    Group group;
    int current_material = -1;
    bool merged_any = false;

    o.clear();

    auto flush = [&]() {
        if (group.pairs.size() == 1)
        {
            const Pair& p = *group.pairs.front();
            PutBytes(o, sgd + p.vuvn_off, p.vuvn_len);
            PutBytes(o, sgd + p.mesh_off, p.mesh_len);
        }
        else if (group.pairs.size() > 1)
        {
            const Pair merged = Merge(group, skin);

            if (merged.kind == VuvnKind::kReferenced && merged.vtype == 3)
            {
                for (const Pair* p : group.pairs)
                {
                    stats->vtype3_batches += p->vtype == 2 ? 1 : 0;
                }
            }

            BuildVuvn(merged, &built);
            PutBytes(o, built.data(), built.size());
            BuildMesh(merged, &built);
            PutBytes(o, built.data(), built.size());

            stats->pairs_merged_away += group.pairs.size() - 1;
            merged_any = true;
        }

        group = Group();

        /// MATERIAL units that turned out not to be inside a group.
        for (const Item* it : pending)
        {
            PutBytes(o, sgd + it->off, it->len);
        }

        pending.clear();
    };

    for (const Item& it : items)
    {
        switch (it.kind)
        {
            case Kind::kMaterial:
                /// Re-selecting the material already in effect is a no-op
                /// (SetMaterialData only depends on the material and the
                /// lights, which change at COORDINATE/BOUNDING_BOX units).
                if (!group.Empty() && it.material == group.material)
                {
                    pending.push_back(&it);
                    break;
                }

                if (!group.Empty())
                {
                    stats->breaks["material change"]++;
                }

                flush();
                PutBytes(o, sgd + it.off, it.len);
                current_material = it.material;
                break;
            case Kind::kPair:
            {
                const Pair* p = &pairs[it.pair];

                if (!group.Empty())
                {
                    const char* why = CanJoin(group, *p, skin, opt);

                    if (why == nullptr)
                    {
                        group.Add(p);
                        pending.clear();
                        break;
                    }

                    stats->breaks[why]++;
                }

                flush();
                group.Start(p, current_material);
                break;
            }
            case Kind::kRaw:
            default:
                if (!group.Empty())
                {
                    stats->breaks["other procedure unit"]++;
                }

                flush();
                PutBytes(o, sgd + it.off, it.len);
                break;
        }
    }

    flush();
    PutBytes(o, sgd + blk.sentinel, 16);
    return merged_any;
}

}// namespace

bool StitchSgd(uint8_t* sgd, size_t size, const StitchOptions& options,
               StitchStats* stats, std::string* log)
{
    StitchStats local;
    SgdLayout layout;
    std::string error;

    local.sgds = 1;

    if (!ReadLayout(sgd, size, &layout, &error))
    {
        local.sgds_invalid = 1;
        stats->Add(local);

        if (log != nullptr)
        {
            *log = error;
        }

        return false;
    }

    const std::vector<uint8_t> original(sgd, sgd + size);
    std::vector<uint8_t> chain;
    bool modified = false;

    for (const Block& b : layout.block)
    {
        if (!b.present)
        {
            continue;
        }

        uint64_t main = 0;
        uint64_t shadow = 0;
        CountMeshes(original.data(), b.units, &main, &shadow);
        local.draws_before += main;
        local.shadow_before += shadow;

        const size_t span = b.end - b.start;

        if (!b.rewritable)
        {
            if (main + shadow != 0)
            {
                local.locked[b.locked_reason]++;
            }

            local.draws_after += main;
            local.shadow_after += shadow;
            continue;
        }

        const uint64_t merged_before = local.pairs_merged_away;
        const uint64_t vtype3_before = local.vtype3_batches;

        if (!RewriteBlock(original.data(), size, b, layout.skin, options,
                          &local, &chain)
            || chain.size() > span)
        {
            local.pairs_merged_away = merged_before;
            local.vtype3_batches = vtype3_before;
            local.draws_after += main;
            local.shadow_after += shadow;
            continue;
        }

        /// Same start offset, so HeaderSection::primitives[] stays valid;
        /// the bytes freed at the tail of the chain are zeroed.
        std::memcpy(sgd + b.start, chain.data(), chain.size());
        std::memset(sgd + b.start + chain.size(), 0, span - chain.size());

        CountBuiltChain(chain, &local.draws_after, &local.shadow_after);
        local.bytes_freed += span - chain.size();
        modified = true;
    }

    if (modified && options.verify)
    {
        std::string difference;

        if (!VerifyEquivalent(original.data(), sgd, size, &difference))
        {
            std::memcpy(sgd, original.data(), size);
            modified = false;
            local.sgds_rolled_back = 1;
            local.draws_after = local.draws_before;
            local.shadow_after = local.shadow_before;
            local.pairs_merged_away = 0;
            local.vtype3_batches = 0;
            local.bytes_freed = 0;

            if (log != nullptr)
            {
                *log = "verification failed, kept original: " + difference;
            }
        }
    }

    local.sgds_modified = modified ? 1 : 0;
    stats->Add(local);
    return modified;
}

}// namespace sgd
