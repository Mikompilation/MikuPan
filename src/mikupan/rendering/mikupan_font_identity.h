#ifndef MIKUPAN_FONT_IDENTITY_H
#define MIKUPAN_FONT_IDENTITY_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace MikuPanFonts
{
constexpr size_t BankCount = 6;
struct Atlas
{
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;
};
using Atlases = std::array<Atlas, BankCount>;
// Original TEX0 bank layouts are the same in the US and EU builds.
constexpr std::array<uint64_t, BankCount> BankTex0 = {
    0x20051005e5422300ULL, 0x2005102625422380ULL, 0x2005104625422480ULL,
    0x2005106625422580ULL, 0x2005108625422680ULL, 0x200510a625422780ULL};
bool Decode(const uint8_t *data, size_t size, const std::array<uint64_t, BankCount> &tex0,
            Atlases &atlases, std::string &error);
std::string Fingerprint(const Atlases &atlases);
}
#endif
