#include "libcdvd.h"

#include "mikupan/mikupan_file_c.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define MIKUPAN_CD_HD_LSN_BASE 0x70000000u
#define MIKUPAN_CD_FILE_LSN_BASE 0x71000000u
#define MIKUPAN_CD_MAX_FILES 32

typedef struct {
    u_int lsn;
    u_int size;
    char path[1024];
} MikuPanCdFile;

static MikuPanCdFile cd_files[MIKUPAN_CD_MAX_FILES];
static u_int cd_next_file_lsn = MIKUPAN_CD_FILE_LSN_BASE;
static int cd_last_error;
/* Opened through SDL_IOFromFile so the emulated CD can read paths that are not
 * plain filesystem paths -- notably Android content:// URIs and assets:// asset
 * paths. stdio fopen() cannot open those, which previously made every sceCdRead
 * fail on Android and hung ICdvdInitOnce in its load-retry loop. */
static SDL_IOStream* cd_stream_file;

static u_char DecToBcd(u_char dec)
{
    return (u_char)(((dec / 10) << 4) | (dec % 10));
}

static int ResolveDataFile(const char* name, char* out, size_t out_size)
{
    if (!MikuPan_ResolveCdPath(name, out, out_size)) {
        return 0;
    }

    return out[0] != '\0';
}

static int ReadFileBytes(const char* path, u_int offset, void* buf, u_int size)
{
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io == NULL) {
        cd_last_error = 1;
        return 0;
    }

    if (offset != 0 && SDL_SeekIO(io, (Sint64)offset, SDL_IO_SEEK_SET) < 0) {
        SDL_CloseIO(io);
        cd_last_error = 1;
        return 0;
    }

    /* Best-effort fill, matching the old stdio read which ignored short reads. */
    u_char* dst = (u_char*)buf;
    size_t remaining = size;
    while (remaining > 0) {
        size_t got = SDL_ReadIO(io, dst, remaining);
        if (got == 0) {
            break;
        }
        dst += got;
        remaining -= got;
    }

    SDL_CloseIO(io);
    cd_last_error = 0;
    return 1;
}

static MikuPanCdFile* FindFileByLsn(u_int lsn)
{
    for (int i = 0; i < MIKUPAN_CD_MAX_FILES; i++) {
        if (cd_files[i].path[0] != '\0' && cd_files[i].lsn == lsn) {
            return &cd_files[i];
        }
    }

    return NULL;
}

static MikuPanCdFile* AddFileMapping(const char* path, u_int lsn, u_int size)
{
    for (int i = 0; i < MIKUPAN_CD_MAX_FILES; i++) {
        if (cd_files[i].path[0] != '\0' && strcmp(cd_files[i].path, path) == 0) {
            return &cd_files[i];
        }
    }

    for (int i = 0; i < MIKUPAN_CD_MAX_FILES; i++) {
        if (cd_files[i].path[0] == '\0') {
            cd_files[i].lsn = lsn;
            cd_files[i].size = size;
            strncpy(cd_files[i].path, path, sizeof(cd_files[i].path) - 1);
            cd_files[i].path[sizeof(cd_files[i].path) - 1] = '\0';
            return &cd_files[i];
        }
    }

    return NULL;
}

int sceCdInit(int init_mode)
{
    (void)init_mode;
    return 1;
}

int sceCdMmode(int media)
{
    (void)media;
    return 1;
}

int sceCdReadClock(sceCdCLOCK* rtc)
{
    SDL_Time now;
    SDL_DateTime dt;
    int year;

    if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &dt, true)) {
        rtc->stat = 0x80;
        return 0;
    }

    year = dt.year - 2000;
    if (year < 0)
        year = 0;
    else if (year > 99)
        year = 99;

    rtc->stat = 0;
    rtc->second = DecToBcd((u_char)dt.second);
    rtc->minute = DecToBcd((u_char)dt.minute);
    rtc->hour = DecToBcd((u_char)dt.hour);
    rtc->pad = 0;
    rtc->day = DecToBcd((u_char)dt.day);
    rtc->month = DecToBcd((u_char)dt.month);
    rtc->year = DecToBcd((u_char)year);

    return 1;
}

int sceCdRead(u_int lsn, u_int sectors, void* buf, sceCdRMode* mode)
{
    char path[1024];
    MikuPanCdFile* file;
    u_int offset = lsn * 0x800u;

    (void)mode;

    if (buf == NULL) {
        cd_last_error = 1;
        return 0;
    }

    if (lsn >= MIKUPAN_CD_HD_LSN_BASE && lsn < MIKUPAN_CD_FILE_LSN_BASE) {
        if (!ResolveDataFile("\\IMG_HD.BIN;1", path, sizeof(path))) {
            cd_last_error = 1;
            return 0;
        }
        offset = (lsn - MIKUPAN_CD_HD_LSN_BASE) * 0x800u;
        return ReadFileBytes(path, offset, buf, sectors * 0x800u);
    }

    file = FindFileByLsn(lsn);
    if (file != NULL) {
        return ReadFileBytes(file->path, 0, buf, sectors * 0x800u);
    }

    if (!ResolveDataFile("\\IMG_BD.BIN;1", path, sizeof(path))) {
        cd_last_error = 1;
        return 0;
    }

    return ReadFileBytes(path, offset, buf, sectors * 0x800u);
}

int sceCdSync(int mode)
{
    (void)mode;
    return 0;
}

int sceCdBreak(void)
{
    return 1;
}

int sceCdGetError(void)
{
    return cd_last_error;
}

int sceCdStRead(u_int size, u_int* buf, u_int mode, u_int* err)
{
    (void)mode;

    if (err != NULL) {
        *err = 0;
    }

    if (cd_stream_file == NULL || buf == NULL) {
        return 0;
    }

    /* Mirror fread(buf, 0x800, size, ...): fill as much as possible and report
     * the number of complete 0x800-byte sectors read. */
    u_char* dst = (u_char*)buf;
    size_t want = (size_t)size * 0x800u;
    size_t total = 0;
    while (total < want) {
        size_t got = SDL_ReadIO(cd_stream_file, dst + total, want - total);
        if (got == 0) {
            break;
        }
        total += got;
    }

    return (int)(total / 0x800u);
}

int sceCdStStop()
{
    if (cd_stream_file != NULL) {
        SDL_CloseIO(cd_stream_file);
        cd_stream_file = NULL;
    }

    return 1;
}

int sceCdDiskReady(int mode)
{
    (void)mode;
    return 2;
}

int sceCdStInit(u_int bufmax, u_int bankmax, u_int iop_bufaddr)
{
    (void)bufmax;
    (void)bankmax;
    (void)iop_bufaddr;
    return 1;
}

int sceCdSearchFile(sceCdlFILE* fp, const char* name)
{
    char path[1024];
    u_int size;
    u_int lsn;

    if (fp == NULL || name == NULL || !ResolveDataFile(name, path, sizeof(path))) {
        return 0;
    }

    size = MikuPan_GetFileSize(path);
    if (size == 0) {
        return 0;
    }

    if (strstr(path, "IMG_HD.BIN") != NULL || strstr(path, "img_hd.bin") != NULL) {
        lsn = MIKUPAN_CD_HD_LSN_BASE;
    } else if (strstr(path, "IMG_BD.BIN") != NULL || strstr(path, "img_bd.bin") != NULL) {
        lsn = 0;
    } else {
        lsn = cd_next_file_lsn++;
    }

    AddFileMapping(path, lsn, size);

    memset(fp, 0, sizeof(*fp));
    fp->lsn = lsn;
    fp->size = size;
    strncpy(fp->name, name, sizeof(fp->name) - 1);
    return 1;
}

int sceCdStStart(u_int lbn, sceCdRMode* mode)
{
    MikuPanCdFile* file;

    (void)mode;

    sceCdStStop();

    file = FindFileByLsn(lbn);
    if (file == NULL) {
        return 0;
    }

    cd_stream_file = SDL_IOFromFile(file->path, "rb");
    return cd_stream_file != NULL;
}
