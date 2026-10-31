#include "mikupan_font_identity.h"
#include "../bake/mikupan_tim2.h"
#include <algorithm>
#include <cstring>
namespace MikuPanFonts
{
namespace
{
// SHA-256, with streaming input so fingerprinting does not duplicate the atlases.
class Sha256
{
    uint32_t state[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint8_t block[64]{};
    uint64_t length = 0;
    size_t used = 0;
    static uint32_t Rotate(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
    void Transform()
    {
        constexpr uint32_t k[64] = {
            0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
            0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
            0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
            0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
            0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
            0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
            0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
            0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
        uint32_t w[64];
        for (unsigned i = 0; i < 16; ++i)
            w[i] = uint32_t(block[i*4]) << 24 | uint32_t(block[i*4+1]) << 16 | uint32_t(block[i*4+2]) << 8 | block[i*4+3];
        for (unsigned i = 16; i < 64; ++i)
        {
            const uint32_t a = w[i-15], b = w[i-2];
            w[i] = w[i-16] + (Rotate(a,7)^Rotate(a,18)^(a>>3)) + w[i-7] + (Rotate(b,17)^Rotate(b,19)^(b>>10));
        }
        uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for (unsigned i = 0; i < 64; ++i)
        {
            const uint32_t t = h + (Rotate(e,6)^Rotate(e,11)^Rotate(e,25)) + ((e&f)^(~e&g)) + k[i] + w[i];
            const uint32_t u = (Rotate(a,2)^Rotate(a,13)^Rotate(a,22)) + ((a&b)^(a&c)^(b&c));
            h=g; g=f; f=e; e=d+t; d=c; c=b; b=a; a=t+u;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
public:
    void Add(const void *source, size_t size)
    {
        const auto *bytes = static_cast<const uint8_t *>(source);
        length += size;
        while (size)
        {
            const size_t n = std::min(size, sizeof(block)-used);
            std::memcpy(block+used, bytes, n); used+=n; bytes+=n; size-=n;
            if (used==sizeof(block)) { Transform(); used=0; }
        }
    }
    void Word(uint32_t value)
    {
        uint8_t bytes[4];
        for (unsigned i=0;i<4;++i) bytes[i]=uint8_t(value>>(i*8));
        Add(bytes,sizeof(bytes));
    }
    std::string Finish()
    {
        const uint64_t bits = length * 8;
        block[used++] = 0x80;
        if (used>56) { std::memset(block+used,0,64-used); Transform(); used=0; }
        std::memset(block+used,0,56-used);
        for (unsigned i=0;i<8;++i) block[63-i]=uint8_t(bits>>(i*8));
        Transform();
        std::string result;
        constexpr char hex[]="0123456789abcdef";
        for (uint32_t word:state)
            for (int shift=28;shift>=0;shift-=4) result+=hex[(word>>shift)&15];
        return result;
    }
};
}
bool Decode(const uint8_t *data, size_t size, const std::array<uint64_t, BankCount> &tex0,
            Atlases &atlases, std::string &error)
{
    using namespace MikuPanBake;
    std::vector<size_t> headers;
    if (!FindTim2Pictures(data,size,&headers)) { error="Invalid font TIM2 pack."; return false; }
    std::vector<Tim2Picture> pictures(headers.size());
    for (size_t i=0;i<headers.size();++i)
        if (!ReadTim2Picture(data+headers[i],size-headers[i],&pictures[i])) { error="Invalid font picture."; return false; }
    Atlases result;
    for (size_t bank=0;bank<BankCount;++bank)
    {
        const auto layout=SplitTex0(tex0[bank]);
        const Tim2Picture *image=nullptr,*palette=nullptr;
        for (const auto &p:pictures)
        {
            const auto t=SplitTex0(p.tex0);
            if (p.image_size && t.tbp0==layout.tbp0 && t.psm==layout.psm) image=&p;
            if (p.clut_size && t.cbp==layout.cbp) palette=&p;
        }
        if (!image || !palette || layout.tw<0 || layout.tw>12 || layout.th<0 || layout.th>12)
        { error="Missing or unsupported font bank "+std::to_string(bank)+"."; return false; }
        auto &atlas=result[bank];
        atlas.width=1<<layout.tw; atlas.height=1<<layout.th;
        atlas.rgba.resize(size_t(atlas.width)*atlas.height*4);
        if (!DecodeTim2Texture(*image,*palette,tex0[bank],atlas.rgba.data()))
        { error="Cannot decode font bank "+std::to_string(bank)+"."; return false; }
    }
    atlases=std::move(result); error.clear(); return true;
}
std::string Fingerprint(const Atlases &atlases)
{
    Sha256 hash;
    constexpr char domain[]="pan-font-v1";
    hash.Add(domain,sizeof(domain)); hash.Word(uint32_t(BankCount));
    for (size_t bank=0;bank<BankCount;++bank)
    {
        const auto &atlas=atlases[bank];
        if (atlas.width<=0 || atlas.height<=0 || atlas.rgba.size()!=size_t(atlas.width)*atlas.height*4) return {};
        hash.Word(uint32_t(bank)); hash.Word(uint32_t(atlas.width)); hash.Word(uint32_t(atlas.height));
        hash.Add(atlas.rgba.data(),atlas.rgba.size());
    }
    return hash.Finish();
}
}
