#ifndef MIKUPAN_SAVE_CODEC_H
#define MIKUPAN_SAVE_CODEC_H
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace MikuPanSave
{
using Bytes = std::vector<uint8_t>;
#ifdef BUILD_EU_VERSION
inline constexpr const char *FolderName = "BESLES-50821zero0";
inline constexpr const char *NativeFolderName = "BASLES-50821zero0";
inline constexpr const char *RegionName = "EU / SLES-50821";
inline constexpr const char *Prefix = "BESLES-50821zero";
inline constexpr const char *NativePrefix = "BASLES-50821zero";
#else
inline constexpr const char *FolderName = "BASLUS-20388zero0";
inline constexpr const char *NativeFolderName = "BASLUS-20388zero0";
inline constexpr const char *RegionName = "US / SLUS-20388";
inline constexpr const char *Prefix = "BASLUS-20388zero";
inline constexpr const char *NativePrefix = "BASLUS-20388zero";
#endif
inline constexpr size_t Ps2Size = 585596;
size_t NativeSize();
bool Empty(const Bytes &data);
bool Checksum(const Bytes &data);
void Seal(Bytes &data);
Bytes EmptySlot(size_t size);
bool Validate(const Bytes &data, std::string &error);
bool ToNative(const Bytes &ps2, Bytes &native, std::string &error);
bool ToPs2(const Bytes &native, Bytes &ps2, std::string &error);
} // namespace MikuPanSave
#endif
