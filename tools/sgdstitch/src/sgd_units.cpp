#include "sgd_units.h"
#include "sgd_io.h"

#include <cstring>

namespace sgd
{
namespace
{

/// VIF1 codes found in SGD packets.
constexpr uint32_t kVifNop = 0x00000000;
constexpr uint32_t kVifStcycl44 = 0x01000404;///< STCYCL CL=4 WL=4
constexpr uint32_t kVifStcycl31 = 0x01000103;///< STCYCL CL=3 WL=1
constexpr uint32_t kVifStcycl21 = 0x01000102;///< STCYCL CL=2 WL=1
constexpr uint32_t kVifStmask = 0x20000000;

/// STMASK patterns, one per data stream.
constexpr uint32_t kMaskVertex = 0x40404040;
constexpr uint32_t kMaskCount = 0xfcfcfcfc;
constexpr uint32_t kMaskSt = 0xd0d0d0d0;
constexpr uint32_t kMaskColor = 0xc0c0c0c0;

/// UNPACK command bytes.
constexpr uint32_t kUnpackS32 = 0x60;
constexpr uint32_t kUnpackV3_32 = 0x68;
constexpr uint32_t kUnpackV4_32 = 0x6c;
constexpr uint32_t kUnpackS32M = 0x70;
constexpr uint32_t kUnpackV2_32M = 0x74;
constexpr uint32_t kUnpackV3_32M = 0x78;

/// GIF tag EOP bit as used by the preset strip headers.
constexpr uint32_t kStripEop = 0x8000;

/// UNPACK with FLG (relative to VIF1_TOPS). NUM and ADDR are hardware bit
/// fields: a stitched unit can be larger than PS2 VU1 memory, in which case
/// the values wrap exactly like the VIF would decode them. MikuPan never reads
/// them back; it relies on the unit header counts instead.
uint32_t Unpack(uint32_t cmd, uint32_t num, uint32_t addr)
{
    return (cmd << 24) | ((num & 0xff) << 16) | 0x8000 | (addr & 0x3ff);
}

uint32_t UnpackNum(uint32_t code)
{
    return (code >> 16) & 0xff;
}

uint32_t UnpackCmd(uint32_t code)
{
    return (code >> 24) & 0x7f;
}

/// VU1 address right after the vertex area (header qword + vertex qwords).
/// The A+D texture packet goes there, the strips follow it.
uint32_t VuDataEnd(const Pair& p)
{
    switch (p.kind)
    {
        case VuvnKind::kReferenced:
        case VuvnKind::kInline:
        case VuvnKind::kPresetSmooth:
            return 1 + 2 * (uint32_t) p.vnum;
        case VuvnKind::kPresetFlat:
            return 1 + (uint32_t) p.nnum + (uint32_t) p.vnum;
    }

    return 0;
}

uint32_t StripBase(const Pair& p)
{
    return VuDataEnd(p) + (p.has_ad ? 2 : 0);
}

/// VU1 qwords used by one strip: header + per-vertex ST/colour/XYZ
/// (textured) or normal/XYZ (untextured).
uint32_t StripQwords(const Pair& p, const Strip& s)
{
    return 1 + (p.family == MeshFamily::kUntextured ? 2 : 3) * s.count;
}

uint8_t ClampQwc(size_t qwc)
{
    return (uint8_t) (qwc > 255 ? 255 : qwc);
}

class Reader
{
  public:
    Reader(const uint8_t* base, size_t begin, size_t end)
        : base_(base), pos_(begin), end_(end)
    {
    }

    bool Word(uint32_t* v)
    {
        if (pos_ + 4 > end_)
        {
            return false;
        }

        *v = Rd32(base_ + pos_);
        pos_ += 4;
        return true;
    }

    bool Expect(uint32_t want)
    {
        uint32_t v;
        return Word(&v) && v == want;
    }

    bool Bytes(size_t n, std::vector<uint8_t>* out)
    {
        if (pos_ + n > end_)
        {
            return false;
        }

        out->assign(base_ + pos_, base_ + pos_ + n);
        pos_ += n;
        return true;
    }

    /// Skips VIF NOPs up to the next qword boundary relative to `unit`.
    bool AlignNops(size_t unit)
    {
        while (((pos_ - unit) & 15) != 0)
        {
            if (!Expect(kVifNop))
            {
                return false;
            }
        }

        return true;
    }

    size_t Pos() const
    {
        return pos_;
    }

  private:
    const uint8_t* base_;
    size_t pos_;
    size_t end_;
};

bool Fail(std::string* reason, const char* why)
{
    if (reason != nullptr)
    {
        *reason = why;
    }

    return false;
}

bool ClassifyMesh(uint8_t mesh_type, Pair* p)
{
    switch (mesh_type)
    {
        case 0x02:
        case 0x0a:
            p->family = MeshFamily::kTextured;
            p->kind = VuvnKind::kReferenced;
            return true;
        case 0x82:
            p->family = MeshFamily::kTextured;
            p->kind = VuvnKind::kInline;
            return true;
        case 0x00:
        case 0x08:
            p->family = MeshFamily::kUntextured;
            p->kind = VuvnKind::kReferenced;
            return true;
        case 0x80:
            p->family = MeshFamily::kUntextured;
            p->kind = VuvnKind::kInline;
            return true;
        case 0x12:
            p->family = MeshFamily::kPreset;
            p->kind = VuvnKind::kPresetSmooth;
            return true;
        case 0x32:
            p->family = MeshFamily::kPreset;
            p->kind = VuvnKind::kPresetFlat;
            return true;
        default:
            return false;
    }
}

bool ParseVuvn(const uint8_t* sgd, Pair* p, std::string* reason)
{
    const uint8_t* u = sgd + p->vuvn_off;
    const size_t len = p->vuvn_len;

    p->vnum = RdS16(u + 8);
    p->nnum = RdS16(u + 10);
    p->vtype = u[13];
    p->vuvn_b14 = u[14];
    p->vuvn_b15 = u[15];

    if (p->vnum <= 0 || p->nnum < 0)
    {
        return Fail(reason, "VUVN vertex count");
    }

    const size_t vnum = (size_t) p->vnum;
    const size_t nnum = (size_t) p->nnum;

    /// Room far-LOD SGDs store positions only (STCYCL CL=2) with tag-only
    /// meshes. They are mapped but never drawn by MikuPan.
    if (p->kind != VuvnKind::kReferenced && len >= 0x2c
        && Rd32(u + 0x28) == kVifStcycl21)
    {
        return Fail(reason, "position-only VUVN (room far LOD)");
    }

    switch (p->kind)
    {
        case VuvnKind::kReferenced:
            if (48 + vnum * 8 > len)
            {
                return Fail(reason, "VUVN too short");
            }

            p->w0 = Rd32(u + 32);
            p->vertices.assign(u + 48, u + 48 + vnum * 8);
            return true;
        case VuvnKind::kInline:
        case VuvnKind::kPresetSmooth:
            if (56 + vnum * 24 > len)
            {
                return Fail(reason, "VUVN too short");
            }

            p->w0 = Rd32(u + 24);
            p->vertices.assign(u + 56, u + 56 + vnum * 24);
            return true;
        case VuvnKind::kPresetFlat:
            if (56 + nnum * 12 + vnum * 12 > len)
            {
                return Fail(reason, "VUVN too short");
            }

            p->w0 = Rd32(u + 24);
            p->normals.assign(u + 56, u + 56 + nnum * 12);
            p->vertices.assign(u + 56 + nnum * 12,
                               u + 56 + nnum * 12 + vnum * 12);
            return true;
    }

    return false;
}

bool ParseTexturedMesh(const uint8_t* sgd, Pair* p, int nmesh)
{
    const size_t m = p->mesh_off;
    Reader r(sgd, m + 16, m + p->mesh_len);
    uint32_t code;

    if (!r.Expect(kVifStcycl44) || !r.Word(&code))
    {
        return false;
    }

    for (uint32_t& w : p->ad)
    {
        if (!r.Word(&w))
        {
            return false;
        }
    }

    p->has_ad = true;

    if (!r.Expect(kVifStmask) || !r.Expect(kMaskCount))
    {
        return false;
    }

    p->strips.resize((size_t) nmesh);

    for (Strip& s : p->strips)
    {
        if (!r.Word(&code) || !r.Word(&s.count))
        {
            return false;
        }
    }

    if (!r.Expect(kVifStmask) || !r.Expect(kMaskSt)
        || !r.Expect(kVifStcycl31))
    {
        return false;
    }

    for (Strip& s : p->strips)
    {
        if (s.count == 0 || s.count > 255 || !r.Word(&code)
            || UnpackNum(code) != s.count || !r.Bytes(s.count * 8, &s.st))
        {
            return false;
        }
    }

    return true;
}

bool ParseUntexturedMesh(const uint8_t* sgd, Pair* p, int nmesh)
{
    const size_t m = p->mesh_off;
    Reader r(sgd, m + 16, m + p->mesh_len);
    uint32_t code;

    if (!r.Expect(kVifStmask) || !r.Expect(kMaskCount))
    {
        return false;
    }

    p->strips.resize((size_t) nmesh);

    for (Strip& s : p->strips)
    {
        if (!r.Word(&code) || !r.Word(&s.count) || s.count == 0)
        {
            return false;
        }
    }

    return true;
}

bool ParsePresetMesh(const uint8_t* sgd, Pair* p, int nmesh)
{
    const size_t m = p->mesh_off;
    Reader r(sgd, m + 32, m + p->mesh_len);
    uint32_t code;
    uint32_t word;

    p->has_ad = Rd32(sgd + m + 32) == kVifStcycl44;

    if (p->has_ad)
    {
        if (!r.Expect(kVifStcycl44) || !r.Word(&code))
        {
            return false;
        }

        for (uint32_t& w : p->ad)
        {
            if (!r.Word(&w))
            {
                return false;
            }
        }

        if (!r.Expect(kVifNop) || !r.Expect(kVifNop))
        {
            return false;
        }
    }

    if (!r.Expect(kVifStmask) || !r.Expect(kMaskColor)
        || !r.Expect(kVifStcycl31))
    {
        return false;
    }

    p->strips.resize((size_t) nmesh);

    for (Strip& s : p->strips)
    {
        if (!r.Word(&code) || UnpackCmd(code) != kUnpackV3_32M)
        {
            return false;
        }

        s.count = UnpackNum(code);

        if (s.count == 0 || !r.Bytes(s.count * 12, &s.colors))
        {
            return false;
        }
    }

    if (!r.AlignNops(m))
    {
        return false;
    }

    for (int k = 0; k < nmesh; k++)
    {
        if (!r.Word(&code) || !r.Word(&word))
        {
            return false;
        }

        if (k == 0 && (!r.Word(&p->tag_w1) || !r.Word(&p->tag_w2)))
        {
            return false;
        }
    }

    if (!r.AlignNops(m) || !r.Expect(kVifStmask) || !r.Expect(kMaskSt)
        || !r.Expect(kVifStcycl31))
    {
        return false;
    }

    for (Strip& s : p->strips)
    {
        if (!r.Word(&code) || UnpackNum(code) != s.count
            || !r.Bytes(s.count * 8, &s.st))
        {
            return false;
        }
    }

    return true;
}

bool SameBytes(const std::vector<uint8_t>& built, const uint8_t* orig,
               size_t len)
{
    return built.size() == len && std::memcmp(built.data(), orig, len) == 0;
}

}// namespace

bool IsMainPassMesh(uint8_t mesh_type)
{
    switch (mesh_type)
    {
        case 0x02:
        case 0x0a:
        case 0x82:
        case 0x12:
        case 0x32:
            return true;
        default:
            return false;
    }
}

bool IsShadowOnlyMesh(uint8_t mesh_type)
{
    return mesh_type == 0x00 || mesh_type == 0x08 || mesh_type == 0x80;
}

bool ParsePair(const uint8_t* sgd, size_t sgd_size, size_t v, size_t m,
               Pair* pair, std::string* reason)
{
    Pair p;
    p.vuvn_off = v;
    p.mesh_off = m;
    p.vuvn_len = Rd32(sgd + v);
    p.mesh_len = Rd32(sgd + m);

    if (v + p.vuvn_len > sgd_size || m + p.mesh_len > sgd_size
        || p.vuvn_len < 16 || p.mesh_len < 32)
    {
        return Fail(reason, "unit out of bounds");
    }

    p.mesh_type = sgd[m + 13];
    p.mesh_pad0 = sgd[m + 12];
    p.mesh_pad1 = sgd[m + 15];

    if (!ClassifyMesh(p.mesh_type, &p))
    {
        return Fail(reason, "mesh type not stitchable");
    }

    const int nmesh = (int) (signed char) sgd[m + 14];

    if (nmesh <= 0)
    {
        return Fail(reason, "strip count");
    }

    if (!ParseVuvn(sgd, &p, reason))
    {
        return false;
    }

    bool ok = false;

    switch (p.family)
    {
        case MeshFamily::kTextured:
            ok = ParseTexturedMesh(sgd, &p, nmesh);
            break;
        case MeshFamily::kUntextured:
            ok = ParseUntexturedMesh(sgd, &p, nmesh);
            break;
        case MeshFamily::kPreset:
            ok = ParsePresetMesh(sgd, &p, nmesh);
            break;
    }

    if (!ok)
    {
        return Fail(reason, "MESH layout not recognised");
    }

    /// The renderer consumes the batch vertices strip after strip, so the
    /// strips must cover the batch exactly for stitched offsets to line up.
    uint32_t covered = 0;
    for (const Strip& s : p.strips)
    {
        covered += s.count;
    }

    if (covered != (uint32_t) p.vnum)
    {
        return Fail(reason, "strips do not cover the vertex batch");
    }

    if (p.kind == VuvnKind::kPresetFlat && p.nnum != nmesh)
    {
        return Fail(reason, "flat normals do not match strips");
    }

    std::vector<uint8_t> built;

    BuildVuvn(p, &built);
    if (!SameBytes(built, sgd + v, p.vuvn_len))
    {
        return Fail(reason, "VUVN does not round-trip");
    }

    BuildMesh(p, &built);
    if (!SameBytes(built, sgd + m, p.mesh_len))
    {
        return Fail(reason, "MESH does not round-trip");
    }

    *pair = std::move(p);
    return true;
}

void BuildVuvn(const Pair& p, std::vector<uint8_t>* out)
{
    std::vector<uint8_t>& o = *out;
    const uint32_t a2 = VuDataEnd(p);
    const uint32_t a = StripBase(p);

    o.assign(16, 0);

    if (p.kind == VuvnKind::kReferenced)
    {
        /// The unit only stores pool offsets; SetVUVNData() expands them to
        /// 2 qwords per vertex in objwork, which is what this UNPACK covers.
        Put32(o, kVifNop);
        Put32(o, kVifNop);
        Put32(o, kVifStcycl44);
        Put32(o, Unpack(kUnpackV4_32, 2 * (uint32_t) p.vnum + 1, 0));
    }
    else
    {
        Put32(o, kVifStcycl44);
        Put32(o, Unpack(kUnpackV4_32, 1, 0));
    }

    Put32(o, p.w0);
    Put32(o, a);
    Put32(o, a2);
    Put32(o, (uint32_t) p.strips.size());

    if (p.kind != VuvnKind::kReferenced)
    {
        const uint32_t elements = p.kind == VuvnKind::kPresetFlat
                                      ? (uint32_t) (p.nnum + p.vnum)
                                      : 2 * (uint32_t) p.vnum;

        Put32(o, kVifStcycl44);
        Put32(o, kVifStmask);
        Put32(o, kMaskVertex);
        Put32(o, Unpack(kUnpackV3_32M, elements, 1));
        PutBytes(o, p.normals.data(), p.normals.size());
    }

    PutBytes(o, p.vertices.data(), p.vertices.size());
    PadTo16(o, 0);

    Wr32(&o[0], (uint32_t) o.size());
    Wr32(&o[4], kPuVuvn);
    Wr16(&o[8], (uint16_t) p.vnum);
    Wr16(&o[10], (uint16_t) p.nnum);
    o[12] = p.kind == VuvnKind::kReferenced
                ? ClampQwc(2 * (size_t) p.vnum + 3)
                : ClampQwc((o.size() - 16) / 16);
    o[13] = p.vtype;
    o[14] = p.vuvn_b14;
    o[15] = p.vuvn_b15;
}

void BuildMesh(const Pair& p, std::vector<uint8_t>* out)
{
    std::vector<uint8_t>& o = *out;
    const uint32_t a2 = VuDataEnd(p);
    const uint32_t a = StripBase(p);
    const size_t n = p.strips.size();
    uint32_t base;
    uint32_t tag_qwc;

    o.assign(p.family == MeshFamily::kPreset ? 32 : 16, 0);

    switch (p.family)
    {
        case MeshFamily::kTextured:
        {
            Put32(o, kVifStcycl44);
            Put32(o, Unpack(kUnpackV4_32, 2, a2));
            for (uint32_t w : p.ad)
            {
                Put32(o, w);
            }

            Put32(o, kVifStmask);
            Put32(o, kMaskCount);

            base = a;
            for (const Strip& s : p.strips)
            {
                Put32(o, Unpack(kUnpackS32M, 1, base));
                Put32(o, s.count);
                base += StripQwords(p, s);
            }

            Put32(o, kVifStmask);
            Put32(o, kMaskSt);
            Put32(o, kVifStcycl31);

            base = a;
            for (const Strip& s : p.strips)
            {
                Put32(o, Unpack(kUnpackV2_32M, s.count, base + 1));
                PutBytes(o, s.st.data(), s.st.size());
                base += StripQwords(p, s);
            }

            PadTo16(o, 0);
            tag_qwc = (uint32_t) (o.size() - 16) / 16;
            break;
        }
        case MeshFamily::kUntextured:
        {
            Put32(o, kVifStmask);
            Put32(o, kMaskCount);

            base = a;
            for (const Strip& s : p.strips)
            {
                Put32(o, Unpack(kUnpackS32M, 1, base));
                Put32(o, s.count);
                base += StripQwords(p, s);
            }

            PadTo16(o, 0);
            tag_qwc = (uint32_t) (o.size() - 16) / 16;
            break;
        }
        case MeshFamily::kPreset:
        default:
        {
            if (p.has_ad)
            {
                Put32(o, kVifStcycl44);
                Put32(o, Unpack(kUnpackV4_32, 2, a2));
                for (uint32_t w : p.ad)
                {
                    Put32(o, w);
                }

                Put32(o, kVifNop);
                Put32(o, kVifNop);
            }

            const size_t color_at = o.size();
            Put32(o, kVifStmask);
            Put32(o, kMaskColor);
            Put32(o, kVifStcycl31);

            base = a;
            for (const Strip& s : p.strips)
            {
                Put32(o, Unpack(kUnpackV3_32M, s.count, base + 2));
                PutBytes(o, s.colors.data(), s.colors.size());
                base += StripQwords(p, s);
            }

            PadTo16(o, 0);

            /// Per-strip GIF tags: the first one is complete, the others only
            /// patch NLOOP (and EOP on the last strip).
            const size_t tag_at = o.size();
            base = a;
            for (size_t k = 0; k < n; k++)
            {
                const Strip& s = p.strips[k];
                const uint32_t nloop = s.count | (k + 1 == n ? kStripEop : 0);

                if (k == 0)
                {
                    Put32(o, Unpack(kUnpackV3_32, 1, base));
                    Put32(o, nloop);
                    Put32(o, p.tag_w1);
                    Put32(o, p.tag_w2);
                }
                else
                {
                    Put32(o, Unpack(kUnpackS32, 1, base));
                    Put32(o, nloop);
                }

                base += StripQwords(p, s);
            }

            PadTo16(o, 0);
            Put32(o, kVifStmask);
            Put32(o, kMaskSt);
            Put32(o, kVifStcycl31);

            const size_t st_at = o.size();
            base = a;
            for (const Strip& s : p.strips)
            {
                Put32(o, Unpack(kUnpackV2_32M, s.count, base + 1));
                PutBytes(o, s.st.data(), s.st.size());
                base += StripQwords(p, s);
            }

            PadTo16(o, 0);

            /// SGDVUMESHDATA_PRESET: offsets are in words. asPad0[0] locates
            /// the A+D packet (0 when the unit has none). sOffsetToST is
            /// relative to the data block (+0x10) and biased by one, see
            /// MikuPan_RenderMeshType0x32; sOffsetToPrim is relative to the
            /// unit and is also what SgPreRender() walks.
            Wr16(&o[16], (uint16_t) (p.has_ad ? 32 / 4 : 0));
            Wr16(&o[18], (uint16_t) (tag_at / 4));
            Wr16(&o[20], (uint16_t) ((st_at - 16) / 4 + 1));
            Wr16(&o[22], (uint16_t) (color_at / 4));
            tag_qwc = (uint32_t) (o.size() - 32) / 16;
            break;
        }
    }

    Wr32(&o[0], (uint32_t) o.size());
    Wr32(&o[4], kPuMesh);
    Wr32(&o[8], tag_qwc);
    o[12] = p.mesh_pad0;
    o[13] = p.mesh_type;
    o[14] = (uint8_t) n;
    o[15] = p.mesh_pad1;
}

}// namespace sgd
