#ifndef SGDSTITCH_SGD_IO_H
#define SGDSTITCH_SGD_IO_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sgd
{

/// SGD data is little-endian (PS2 EE). Read/write byte-wise so the tool also
/// works on big-endian hosts and on unaligned offsets.
inline uint32_t Rd32(const uint8_t* p)
{
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16)
           | ((uint32_t) p[3] << 24);
}

inline uint16_t Rd16(const uint8_t* p)
{
    return (uint16_t) (p[0] | (p[1] << 8));
}

inline int16_t RdS16(const uint8_t* p)
{
    return (int16_t) Rd16(p);
}

inline void Wr32(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t) v;
    p[1] = (uint8_t) (v >> 8);
    p[2] = (uint8_t) (v >> 16);
    p[3] = (uint8_t) (v >> 24);
}

inline void Wr16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t) v;
    p[1] = (uint8_t) (v >> 8);
}

inline void Put32(std::vector<uint8_t>& out, uint32_t v)
{
    const size_t at = out.size();
    out.resize(at + 4);
    Wr32(&out[at], v);
}

inline void PutBytes(std::vector<uint8_t>& out, const uint8_t* p, size_t n)
{
    out.insert(out.end(), p, p + n);
}

/// Appends VIF NOP words (zero) until the stream is qword aligned relative to
/// `base`. SGD procedure units always start on a 16-byte boundary.
inline void PadTo16(std::vector<uint8_t>& out, size_t base)
{
    while (((out.size() - base) & 15) != 0)
    {
        Put32(out, 0);
    }
}

/// 64-bit FNV-1a, used by the verifier to fingerprint draw records.
class Fnv64
{
  public:
    void Add(const void* data, size_t size)
    {
        const uint8_t* p = (const uint8_t*) data;
        for (size_t i = 0; i < size; i++)
        {
            hash_ ^= p[i];
            hash_ *= 1099511628211ULL;
        }
    }

    void Add32(uint32_t v)
    {
        uint8_t b[4];
        Wr32(b, v);
        Add(b, 4);
    }

    uint64_t Value() const
    {
        return hash_;
    }

  private:
    uint64_t hash_ = 1469598103934665603ULL;
};

}// namespace sgd

#endif// SGDSTITCH_SGD_IO_H
