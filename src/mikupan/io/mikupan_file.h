#ifndef MIKUPAN_FILE_LOADING_H
#define MIKUPAN_FILE_LOADING_H

#include <cstdint>
#include <cstddef>
#include <typedefs.h>
#include <string>

struct SDL_IOStream;

/// What the data setup screen can point the game at.
enum
{
    MIKUPAN_DATA_SOURCE_DISC_IMAGE,
    MIKUPAN_DATA_SOURCE_IMG_HD,
    MIKUPAN_DATA_SOURCE_IMG_BD,
    MIKUPAN_DATA_SOURCE_COUNT
};

typedef struct
{
    unsigned char Resv2,Sec,Min,Hour;
    unsigned char Day,Month;
    unsigned short Year;
} MikuPan_McStDateTime;

TYPEDEF_ALIGNED(64, struct
{
    MikuPan_McStDateTime _Create;
    MikuPan_McStDateTime _Modify;
    unsigned int FileSizeByte;
    unsigned short AttrFile;
    unsigned short Reserve1;
    unsigned int Reserve2;
    unsigned int PdaAplNo;
    unsigned char EntryName[32];
} MikuPan_McTblGetDir);



extern "C" {
void MikuPan_LoadImgHdFile();
void MikuPan_ServiceMissingDataFolderDialog();
void MikuPan_RequestDataFolderSelection(const char *missing_path);
bool MikuPan_HasRequiredDataFiles(char *missing_file, size_t missing_file_size);
/// Shows the file picker for a MIKUPAN_DATA_SOURCE_*. The choice is checked,
/// and saved to the configuration if it is good, by
/// MikuPan_ServiceDataSourceBrowser() on the main thread.
void MikuPan_BrowseForDataSource(int source);
void MikuPan_ServiceDataSourceBrowser();
/// The release this build plays, as a player would call it.
const char *MikuPan_GetRequiredDiscName();
void MikuPan_ReadFullFile(const char *filename, char *buffer);
void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
void MikuPan_BufferFile(int sector, int size, int64_t address);
u_int MikuPan_GetFileSize(const char *filename);
/// Size in bytes of a file resolved against the configured data folder (the
/// same root used for IMG_HD.BIN/IMG_BD.BIN), or 0 if it can't be resolved.
/// Unlike MikuPan_LoadImgHdFile()/MikuPan_ReadFileInArchive(), this never
/// pops the "missing data folder" dialog -- callers that treat the file as
/// optional should decide for themselves how to react to a 0 result.
u_int MikuPan_GetDataFileSize(const char *relative_name);
/// Reads `size` bytes starting at `offset` from a file resolved against the
/// configured data folder into a plain host buffer. Returns false (without
/// prompting for a data folder) if the file is missing or the read is
/// short.
bool MikuPan_ReadDataFileRange(const char *relative_name, int64_t offset,
                               void *buffer, size_t size);
/// Opens a file of the configured data folder for repeated range reads
/// (MikuPan_ReadDataFileRange() reopens the file on every call). Works with
/// plain paths and Android content URIs. Returns NULL (without prompting
/// for a data folder) when the file cannot be opened.
void *MikuPan_OpenDataFile(const char *relative_name);
/// Reads exactly `size` bytes at `offset`; false on a short read.
bool MikuPan_ReadOpenDataFile(void *file, int64_t offset, void *buffer,
                              size_t size);
void MikuPan_CloseDataFile(void *file);
/// Opens for reading any path this layer hands out: a plain path, an Android
/// content URI, or a file on the mounted disc image. See MikuPan_ResolveCdPath.
SDL_IOStream *MikuPan_OpenStream(const char *path);
u_char MikuPan_OpenFile(const char *filename, void *buffer, int size);
u_char MikuPan_SaveFile(const char *filename, void *buffer, int size);
bool MikuPan_ResolveCdPath(const char* path, char* buffer, size_t buffer_size);
bool MikuPan_ResolveBasePath(const char* path, char* buffer, size_t buffer_size);
/// Directory that contains the bundled `resources/` folder, with a trailing
/// separator. Wraps SDL_GetBasePath() and corrects it on platforms where that
/// does not point at the executable's own directory (see the implementation
/// for the macOS case). Never returns NULL.
const char *MikuPan_GetBaseDirectory(void);
u_char MikuPan_ReadFile(const char *filename, void *buffer, int size);
u_char MikuPan_WriteFile(const char *filename, const void *buffer, int size);
u_char MikuPan_CreateFolder(const char *folder);
u_char MikuPan_FolderExists(const char *folder);
int MikuPan_GetListFiles(const char *folder, MikuPan_McTblGetDir *table, int max_entries);
}

std::string MikuPan_GetRelativePath(const char* path);
/// The file a MIKUPAN_DATA_SOURCE_* currently stands for, empty when it has
/// none.
std::string MikuPan_GetDataSourcePath(int source);
/// Why the data that was found or picked cannot be used, in words for the
/// player. Empty when there is nothing to report.
std::string MikuPan_GetDataProblem();

#endif//MIKUPAN_FILE_LOADING_H
