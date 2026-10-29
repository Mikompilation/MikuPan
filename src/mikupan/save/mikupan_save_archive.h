#ifndef MIKUPAN_SAVE_ARCHIVE_H
#define MIKUPAN_SAVE_ARCHIVE_H
#include "mikupan_memory_card.h"
namespace MikuPanSave
{
enum class SaveArchiveFormat
{
    None,
    Psu,
    Max,
    Xps,
    Cbs
};
bool ReadSaveArchive(const Bytes &data, std::string &directory, SaveFiles &files,
                     std::string &error);
bool WriteSaveArchive(const std::string &directory, const SaveFiles &files,
                      SaveArchiveFormat format, Bytes &data, std::string &error);
} // namespace MikuPanSave
#endif
