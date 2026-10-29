#include "sgd_verify.h"
#include "sgd_io.h"
#include "sgd_layout.h"
#include "sgd_units.h"

#include <cstring>

namespace sgd
{
namespace
{

/// Record tags so different record kinds can never collide by layout.
constexpr uint32_t kRecordRaw = 0x21574152;     // "RAW!"
constexpr uint32_t kRecordTriangle = 0x21495254;// "TRI!"

struct SimState
{
    uint32_t block = 0;
    int material = -1;
    bool tex_bound = false;
    uint8_t tex[8] = {};
};

bool Error(std::string* error, const std::string& why)
{
    if (error != nullptr)
    {
        *error = why;
    }

    return false;
}

void RecordRaw(DrawStream* out, const SimState& st, const uint8_t* sgd,
               const Unit& u)
{
    Fnv64 h;
    h.Add32(kRecordRaw);
    h.Add32(st.block);
    h.Add32(u.type);
    h.Add(sgd + u.off, u.len);
    out->records.push_back(h.Value());
}

void CountDraw(DrawStream* out, uint8_t mesh_type)
{
    if (IsMainPassMesh(mesh_type))
    {
        out->main_draws++;
    }
    else if (IsShadowOnlyMesh(mesh_type))
    {
        out->shadow_draws++;
    }
}

/// MikuPan_FixUV: a V of exactly 0x00000001 is a PS2 marker, the renderer
/// replaces it with the V two vertices back in the same strip.
void FixUv(std::vector<uint8_t>* st, uint32_t count)
{
    for (uint32_t i = 2; i < count; i++)
    {
        if (Rd32(&(*st)[i * 8 + 4]) == 1)
        {
            std::memcpy(&(*st)[i * 8 + 4], &(*st)[(i - 2) * 8 + 4], 4);
        }
    }
}

/// MikuPan_FixColors: same marker convention on the red channel.
void FixColors(std::vector<uint8_t>* col, uint32_t count)
{
    for (uint32_t i = 2; i < count; i++)
    {
        if (Rd32(&(*col)[i * 12]) == 1)
        {
            std::memcpy(&(*col)[i * 12], &(*col)[(i - 2) * 12], 12);
        }
    }
}

bool IsReferenced(uint8_t mesh_type)
{
    return mesh_type == 0x02 || mesh_type == 0x0a || mesh_type == 0x00
           || mesh_type == 0x08;
}

/// Skinning identity of a referenced vertex: which pool/path it goes
/// through and, when the plain path can run, which bone pair moves it.
void HashSkin(Fnv64* h, const uint8_t* sgd, size_t size, const SkinInfo& skin,
              uint8_t vtype, const uint8_t* refs, uint32_t index)
{
    if (vtype != 2 && vtype != 3)
    {
        h->Add32(0x100 + vtype);
        return;
    }

    h->Add32(0x200);

    if (!skin.plain_possible)
    {
        return;
    }

    /// vtype 2: SkinCalcLoopType2 / SkinPreparePlain use the first vertex of
    /// the batch for every vertex; vtype 3 uses each vertex's own record.
    const uint64_t ref = Rd32(refs + (vtype == 2 ? 0 : (size_t) index * 8));
    const uint64_t rec = (uint64_t) skin.weighted_vertex + ref * 32;

    if (skin.weighted_vertex == 0 || rec + 32 > size)
    {
        h->Add32(0xffffffff);
        return;
    }

    h->Add32(sgd[rec + 0x1c] | (sgd[rec + 0x1d] << 8));
}

bool SimulatePair(const uint8_t* sgd, size_t size, const SkinInfo& skin,
                  size_t v, size_t m, SimState* st, DrawStream* out,
                  std::string* error)
{
    const uint8_t* vu = sgd + v;
    const uint8_t* mu = sgd + m;
    const uint8_t* vend = vu + Rd32(vu);
    const uint8_t* mend = mu + Rd32(mu);
    const uint8_t mt = mu[13];
    const int nmesh = (int) (signed char) mu[14];
    const int vnum = RdS16(vu + 8);
    const int nnum = RdS16(vu + 10);
    const uint8_t vtype = vu[13];

    auto in_mesh = [&](const uint8_t* p, size_t n) {
        return p >= mu && p + n <= mend;
    };
    auto in_vuvn = [&](const uint8_t* p, size_t n) {
        return p >= vu && p + n <= vend;
    };

    if (nmesh <= 0 || nmesh > kMaxStripsPerMesh)
    {
        return Error(error, "strip count outside the signed char range");
    }

    if (IsReferenced(mt) && vnum > kMaxReferencedVertsHardLimit)
    {
        return Error(error, "referenced batch too large for objwork");
    }

    std::vector<uint32_t> counts((size_t) nmesh);
    std::vector<const uint8_t*> st_at((size_t) nmesh, nullptr);
    std::vector<const uint8_t*> col_at((size_t) nmesh, nullptr);
    bool textured = false;

    switch (mt)
    {
        case 0x02:
        case 0x0a:
        case 0x82:
        {
            /// MikuPan_RenderMeshType0x2 / 0x82: SGDVUMESHPOINTNUM at
            /// &pPUHead[4], the ST register set right after, then the ST
            /// blocks; TEX0 at pProcData + 0x18.
            const uint8_t* info = mu + 64;
            if (!in_mesh(info, (size_t) nmesh * 8 + 12))
            {
                return Error(error, "point counts out of bounds");
            }

            for (int k = 0; k < nmesh; k++)
            {
                counts[k] = Rd32(info + k * 8 + 4);
            }

            const uint8_t* s = info + (size_t) nmesh * 8 + 12;
            for (int k = 0; k < nmesh; k++)
            {
                if (!in_mesh(s, 4 + (size_t) counts[k] * 8))
                {
                    return Error(error, "ST data out of bounds");
                }

                st_at[k] = s + 4;
                s += 4 + (size_t) counts[k] * 8;
            }

            std::memcpy(st->tex, mu + 0x28, 8);
            st->tex_bound = true;
            textured = true;
            break;
        }
        case 0x00:
        case 0x08:
        case 0x80:
        {
            /// Shadow silhouettes: SGDVUMESHPOINTNUM at word 6.
            const uint8_t* info = mu + 24;
            if (!in_mesh(info, (size_t) nmesh * 8))
            {
                return Error(error, "point counts out of bounds");
            }

            for (int k = 0; k < nmesh; k++)
            {
                counts[k] = Rd32(info + k * 8 + 4);
            }
            break;
        }
        case 0x12:
        case 0x32:
        {
            /// MikuPan_RenderMeshType0x32 / SgPreRender: colours found by
            /// scanning for UNPACK codes from sOffsetToPrim, ST blocks from
            /// sOffsetToST; TEX0 only bound when it precedes the colours.
            const long off_st = RdS16(mu + 20);
            const long off_prim = RdS16(mu + 22);

            if (off_st <= 0 || off_prim <= 0)
            {
                return Error(error, "preset offsets");
            }

            const uint8_t* s = mu + 16 + (off_st - 1) * 4;
            const uint8_t* pv = mu + off_prim * 4;

            if (mu + 0x38 < pv)
            {
                std::memcpy(st->tex, mu + 0x38, 8);
                st->tex_bound = true;
            }

            for (int k = 0; k < nmesh; k++)
            {
                while (in_mesh(pv, 4) && (Rd32(pv) & 0x60000000) != 0x60000000)
                {
                    pv += 4;
                }

                if (!in_mesh(pv, 4))
                {
                    return Error(error, "colour unpack not found");
                }

                counts[k] = (Rd32(pv) >> 16) & 0xff;
                if (!in_mesh(pv + 4, (size_t) counts[k] * 12))
                {
                    return Error(error, "colour data out of bounds");
                }

                col_at[k] = pv + 4;
                pv += 4 + (size_t) counts[k] * 12;
            }

            for (int k = 0; k < nmesh; k++)
            {
                if (!in_mesh(s, 4 + (size_t) counts[k] * 8))
                {
                    return Error(error, "ST data out of bounds");
                }

                st_at[k] = s + 4;
                s += 4 + (size_t) counts[k] * 8;
            }

            textured = true;
            break;
        }
        default:
            return Error(error, "unexpected mesh type");
    }

    uint64_t covered = 0;
    for (uint32_t c : counts)
    {
        covered += c;
    }

    if (covered != (uint64_t) vnum)
    {
        return Error(error, "strips do not cover the vertex batch");
    }

    const uint8_t* refs = vu + 48;
    const uint8_t* records = vu + 56;
    const uint8_t* flat_normals = vu + 56;
    const uint8_t* flat_positions = vu + 56 + (size_t) nnum * 12;

    if (IsReferenced(mt) ? !in_vuvn(refs, (size_t) vnum * 8)
        : mt == 0x32 ? !in_vuvn(flat_normals, (size_t) (nnum + vnum) * 12)
                     : !in_vuvn(records, (size_t) vnum * 24))
    {
        return Error(error, "vertex data out of bounds");
    }

    if (mt == 0x32 && nnum < nmesh)
    {
        return Error(error, "missing flat normals");
    }

    uint32_t base = 0;
    std::vector<uint8_t> stc;
    std::vector<uint8_t> colc;

    for (int k = 0; k < nmesh; k++)
    {
        const uint32_t c = counts[k];

        stc.clear();
        colc.clear();

        if (st_at[k] != nullptr)
        {
            stc.assign(st_at[k], st_at[k] + (size_t) c * 8);
            if (mt & 0x02)
            {
                FixUv(&stc, c);
            }
        }

        if (col_at[k] != nullptr)
        {
            colc.assign(col_at[k], col_at[k] + (size_t) c * 12);
            if (mt & 0x10)
            {
                FixColors(&colc, c);
            }
        }

        /// MikuPan_SetTriangleIndex: alternating winding per strip.
        for (uint32_t j = 0; j + 2 < c; j++)
        {
            const uint32_t tri[3] = {(j & 1) ? j + 1 : j, (j & 1) ? j : j + 1,
                                     j + 2};
            Fnv64 h;

            h.Add32(kRecordTriangle);
            h.Add32(st->block);
            h.Add32(mt);
            h.Add32((uint32_t) st->material);

            if (textured)
            {
                h.Add32(st->tex_bound ? 1 : 0);
                h.Add(st->tex, 8);
            }

            for (uint32_t t : tri)
            {
                const uint32_t i = base + t;

                if (IsReferenced(mt))
                {
                    h.Add(refs + (size_t) i * 8, 8);
                    HashSkin(&h, sgd, size, skin, vtype, refs, i);
                }
                else if (mt == 0x32)
                {
                    h.Add(flat_positions + (size_t) i * 12, 12);
                    h.Add(flat_normals + (size_t) k * 12, 12);
                }
                else
                {
                    h.Add(records + (size_t) i * 24, 24);
                }

                if (!stc.empty())
                {
                    h.Add(&stc[(size_t) t * 8], 8);
                }

                if (!colc.empty())
                {
                    h.Add(&colc[(size_t) t * 12], 12);
                }
            }

            out->records.push_back(h.Value());
            out->triangles++;
        }

        base += c;
    }

    CountDraw(out, mt);
    return true;
}

}// namespace

bool SimulateSgd(const uint8_t* sgd, size_t size, DrawStream* stream,
                 std::string* error)
{
    SgdLayout layout;

    if (!ReadLayout(sgd, size, &layout, error))
    {
        return false;
    }

    SimState st;

    for (uint32_t b = 0; b < layout.blocks; b++)
    {
        const std::vector<Unit>& units = layout.block[b].units;
        st.block = b;

        for (size_t i = 0; i < units.size();)
        {
            const Unit& u = units[i];

            if (u.type == kPuVuvn && i + 1 < units.size()
                && units[i + 1].type == kPuMesh)
            {
                const Unit& mesh = units[i + 1];
                Pair pair;

                if (ParsePair(sgd, size, u.off, mesh.off, &pair, nullptr))
                {
                    std::string why;
                    if (!SimulatePair(sgd, size, layout.skin, u.off, mesh.off,
                                      &st, stream, &why))
                    {
                        return Error(error, "block " + std::to_string(b)
                                                + ": " + why);
                    }
                }
                else
                {
                    RecordRaw(stream, st, sgd, u);
                    RecordRaw(stream, st, sgd, mesh);
                    CountDraw(stream, sgd[mesh.off + 13]);
                }

                i += 2;
                continue;
            }

            if (u.type == kPuMaterial && u.len >= 16)
            {
                st.material = (int) Rd32(sgd + u.off + 8);
                i++;
                continue;
            }

            if (u.type == kPuMesh)
            {
                CountDraw(stream, sgd[u.off + 13]);
            }

            RecordRaw(stream, st, sgd, u);
            i++;
        }
    }

    return true;
}

bool VerifyEquivalent(const uint8_t* before, const uint8_t* after, size_t size,
                      std::string* difference)
{
    SgdLayout layout;
    std::string error;

    if (!ReadLayout(before, size, &layout, &error))
    {
        return Error(difference, "original does not parse: " + error);
    }

    /// Only bytes inside block chains may change.
    std::vector<uint8_t> in_chain(size, 0);
    for (const Block& b : layout.block)
    {
        if (b.present)
        {
            std::memset(&in_chain[b.start], 1, b.end - b.start);
        }
    }

    for (size_t i = 0; i < size; i++)
    {
        if (!in_chain[i] && before[i] != after[i])
        {
            return Error(difference, "byte outside the chains changed at +"
                                         + std::to_string(i));
        }
    }

    DrawStream a;
    DrawStream b;

    if (!SimulateSgd(before, size, &a, &error))
    {
        return Error(difference, "original: " + error);
    }

    if (!SimulateSgd(after, size, &b, &error))
    {
        return Error(difference, "stitched: " + error);
    }

    const size_t n = a.records.size() < b.records.size() ? a.records.size()
                                                         : b.records.size();
    for (size_t i = 0; i < n; i++)
    {
        if (a.records[i] != b.records[i])
        {
            return Error(difference,
                         "draw record " + std::to_string(i) + " differs");
        }
    }

    if (a.records.size() != b.records.size() || a.triangles != b.triangles)
    {
        return Error(difference, "different number of draw records");
    }

    return true;
}

}// namespace sgd
