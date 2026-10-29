#ifndef MIKUPAN_MEMORY_CARD_H
#define MIKUPAN_MEMORY_CARD_H
#include "mikupan_save_codec.h"
#include <map>
namespace MikuPanSave
{
using SaveFiles = std::map<std::string, Bytes>;
// Original implementation from the documented PS2 filesystem format; no
// emulator code.
bool ReadMemoryCard(const Bytes& image, SaveFiles& files, bool& found,
                    std::string& error, const char* folder = FolderName);
// An empty base creates an 8 MB card. Output is always a separate raw or
// ECC-equipped image.
bool WriteMemoryCard(const Bytes& base, const SaveFiles& files, Bytes& output,
                     std::string& error, bool ecc = true,
                     const char* folder = FolderName);
}// namespace MikuPanSave
#endif
