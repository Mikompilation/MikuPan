#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "mc_exec.h"

#include "main/glob.h"
#include "mc/mc.h"
#include "mc/mc_acs.h"
#include "mc/mc_icon.h"
#include "mc/mc_main.h"
#include "mikupan/mikupan_memory.h"
#include "outgame/btl_mode/btl_mode.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FRAMES_PER_SECOND  (60)
#define SECONDS_PER_MINUTE (60)
#define SECONDS_PER_HOUR   (60 * SECONDS_PER_MINUTE)

#ifdef BUILD_EU_VERSION
#define KEYCODE "BA"
#define PRODUCT_CODE "SLES-50821"
#else
#define KEYCODE "BA"
#define PRODUCT_CODE "SLUS-20388"
#endif

typedef struct
{
    u_char rank;
    u_char pad;
    u_short best_time;
    int32_t best_shot;
} MC_LEGACY_STAGE_WRK;

typedef char mc_legacy_stage_wrk_must_be_8_bytes[
    (sizeof(MC_LEGACY_STAGE_WRK) == 8) ? 1 : -1];

static u_int mcGetLegacyGameFileSize(void)
{
    u_int legacy_stage_size =
        (u_int)(sizeof(MC_LEGACY_STAGE_WRK)
                * (sizeof(stage_wrk) / sizeof(stage_wrk[0])));

    if (sizeof(stage_wrk) <= legacy_stage_size)
    {
        return mc_game_size;
    }

    return mc_game_size - ((u_int)sizeof(stage_wrk) - legacy_stage_size);
}

static int mcIsValidGameFileSize(u_int size)
{
    return size == mc_game_size || size == mcGetLegacyGameFileSize();
}

static u_int mcFindListedFileSize(const char *name)
{
    const char *entry_name = strrchr(name, '/');
    u_int i;

    if (entry_name != NULL)
    {
        entry_name++;
    }
    else
    {
        entry_name = name;
    }

    for (i = 0; i < mc_ctrl.dir.file_num; i++)
    {
        if (strcmp((char *)mc_ctrl.dir.table[i].EntryName, entry_name) == 0)
        {
            return mc_ctrl.dir.table[i].FileSizeByte;
        }
    }

    return 0;
}

static void mcLoadLegacyStageWrk(const u_char *src)
{
    const MC_LEGACY_STAGE_WRK *legacy = (const MC_LEGACY_STAGE_WRK *)src;
    u_int i;

    for (i = 0; i < sizeof(stage_wrk) / sizeof(stage_wrk[0]); i++)
    {
        stage_wrk[i].rank = legacy[i].rank;
        stage_wrk[i].best_time = legacy[i].best_time;
        stage_wrk[i].best_shot = legacy[i].best_shot;
    }
}

static u_char *mcResolveDataAddress(const MC_DATA_STR *data_str)
{
    switch (data_str->addr_kind)
    {
    case MC_DATA_ADDR_HOST:
        return (u_char *)data_str->addr;
    case MC_DATA_ADDR_PS2:
        return (u_char *)MikuPan_GetHostPointer((int)data_str->addr);
    }

    return NULL;
}

void mcSetPathDir(char file_no)
{
    if (mc_ctrl.mode == MC_MODE_GAMESAVE || mc_ctrl.mode == MC_MODE_GAMELOAD || mc_ctrl.mode == MC_MODE_STARTCHECK)
    {
        sprintf(mc_ctrl.rw.dir, "%s0", KEYCODE PRODUCT_CODE "zero");
        sprintf(mc_ctrl.rw.path, "%s", "/" KEYCODE PRODUCT_CODE "zero0/");
    }
    else
    {
        sprintf(mc_ctrl.rw.dir, "%s%d", KEYCODE PRODUCT_CODE "albm", file_no);
        sprintf(mc_ctrl.rw.path, "%s%d/", "/" KEYCODE PRODUCT_CODE "albm", file_no);
    }
}

void mcMakeGameFileName(char *fname, char file_id)
{
    sprintf(fname, "%s%s%d", mc_ctrl.rw.path, KEYCODE PRODUCT_CODE "zero", file_id);
}

void mcMakeAlbumFileName(char *fname, char file_id)
{
    sprintf(fname, "%s%s%d", mc_ctrl.rw.path, KEYCODE PRODUCT_CODE "albm", file_id);

    if (0)
    {
        // most likely some optimized out debug code
        while(1) {};
    }
}

void mcMakeIconsysFileName(char *fname)
{
    sprintf(fname, "%s%s", mc_ctrl.rw.path, "icon.sys");
}

void mcMakeIconDataFileName(char *fname, char file_id)
{
    sprintf(fname, "%s%s", mc_ctrl.rw.path, mc_iconName[file_id]);
}

char mcCheckEmpty(u_int offset)
{
    u_int size;
    char empty;

    if (
        mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_CAP_ALBUM_LOAD || mc_ctrl.mode == MC_SEL_SLOT ||
        mc_ctrl.mode == MC_SEL_START || mc_ctrl.mode == MC_SEL_FILE
    )
    {
        return 0;
    }

    if (mc_ctrl.mode == MC_INIT || mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_SEL_LOAD)
    {
        size = 0x6fd; // sizeof ??
    }
    else
    {
        size = 0x578; // sizeof ??
    }

    if (size < offset)
    {
        size = 0;
    } else
    {
        size = size - offset;
    }

    empty = mc_ctrl.card[0].free < size;

    return empty;
}

void mcAcsFileReq(char type, int mode, int header)
{
    u_int listed_size;

    switch(type) {
    case MC_FILE_ICONSYS:
        mcMakeIconsysFileName(mc_ctrl.rw.name);

        mc_ctrl.rw.step = 0;
        mc_ctrl.rw.mode = mode;
        mc_ctrl.rw.buf = (char *)&mc_ctrl;
        mc_ctrl.rw.size = 0x3c4; // sizeof ??
    break;
    case MC_FILE_ICONDATA1:
    case MC_FILE_ICONDATA2:
    case MC_FILE_ICONDATA3:
        mcMakeIconDataFileName(mc_ctrl.rw.name, type - 1);

        mc_ctrl.rw.mode = mode;
        mc_ctrl.rw.step = 0;
        mc_ctrl.rw.buf = (char *)mc_ctrl.work_addr;

        if (mc_ctrl.mode == MC_INIT || mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_SEL_LOAD)
        {
            mc_ctrl.rw.size = 0xf850; // sizeof ??
        }
        else
        {
            mc_ctrl.rw.size = 0x104b0; // sizeof ??
        }
    break;
    case MC_FILE_GAMEDATA1:
    case MC_FILE_GAMEDATA2:
    case MC_FILE_GAMEDATA3:
        mcMakeGameFileName(mc_ctrl.rw.name, type - MC_FILE_GAMEDATA1);

        mc_ctrl.rw.step = 0;
        mc_ctrl.rw.mode = mode;
        mc_ctrl.rw.buf = (char *)mc_ctrl.work_addr;
        mc_ctrl.rw.header_flg = header;

        if (header != 0)
        {
            mc_ctrl.rw.size = 0x30; // sizeof(MC_HEADER) is just 0x20 ...
        }
        else
        {
            mc_ctrl.rw.size = mc_game_size;
            if (mode == 1)
            {
                listed_size = mcFindListedFileSize(mc_ctrl.rw.name);
                if (mcIsValidGameFileSize(listed_size))
                {
                    mc_ctrl.rw.size = listed_size;
                }
            }
        }
    break;
    case MC_FILE_ALBUMDATA1:
    case MC_FILE_ALBUMDATA2:
    case MC_FILE_ALBUMDATA3:
    case MC_FILE_ALBUMDATA4:
    case MC_FILE_ALBUMDATA5:
        mcMakeAlbumFileName(mc_ctrl.rw.name, type - MC_FILE_ALBUMDATA1);

        mc_ctrl.rw.step = 0;
        mc_ctrl.rw.mode = mode;
        mc_ctrl.rw.buf = (char *)mc_ctrl.work_addr;
        mc_ctrl.rw.header_flg = header;

        if (header != 0)
        {
            mc_ctrl.rw.size = 0x30; // sizeof(MC_HEADER) is just 0x20 ...
        }
        else
        {
            mc_ctrl.rw.size = mc_album_size;
        }
    break;
    }
}

char mcWriteFile()
{
    switch(mc_ctrl.rw.step)
    {
    case 0:
        mcAcsReq(MC_FUNC_OPEN);

        mc_ctrl.rw.step = 1;
    break;
    case 1:
        if (mcAcsMain() != 0)
        {
            if (mc_ctrl.acs.sta != 0)
            {
                return 1;
            }

            mc_ctrl.rw.step = 2;
        }
    break;
    case 2:
        mcAcsReq(MC_FUNC_SAVE);
        mc_ctrl.rw.step = 3;
    break;
    case 3:
        if (mcAcsMain() != 0)
        {
            if (mc_ctrl.acs.sta != 0)
            {
                return 1;
            }

            mc_ctrl.rw.step = 4;
        }
    break;
    case 4:
        mcAcsReq(MC_FUNC_CLOSE);

        mc_ctrl.rw.step = 5;
    break;
    case 5:
        if (mcAcsMain() != 0)
        {
            return 1;
        }
    break;
    }

    return 0;
}

char mcReadFile()
{
    switch(mc_ctrl.rw.step)
    {
    case 0:
        mcAcsReq(MC_FUNC_OPEN);

        mc_ctrl.rw.step = 1;
    break;
    case 1:
        if (mcAcsMain() != 0)
        {
            if (mc_ctrl.acs.sta != 0)
            {
                return 1;
            }

            mc_ctrl.rw.step = 2;
        }
        break;
    case 2:
        mcAcsReq(MC_FUNC_LOAD);

        mc_ctrl.rw.step = 3;
    break;
    case 3:
        if (mcAcsMain() != 0)
        {
            if (mc_ctrl.acs.sta != 0)
            {
                return 1;
            }

            mc_ctrl.rw.step = 4;
        }
        break;
    case 4:
        mcAcsReq(MC_FUNC_CLOSE);

        mc_ctrl.rw.step = 5;
    break;
    case 5:
        if (mcAcsMain() != 0)
        {
            return 1;
        }
    break;
    }

    return 0;
}

char mcCheckFileList()
{
    u_int i;
    u_int file_num;
    u_int size;
    char str[256];

    for (i = 0; i < mc_ctrl.dir.file_num; i++)
    {
       // debug code ?
    }

    // compute the size of the directory
    mc_ctrl.dir_size = 0;

    // first: sum the size of each file
    for (i = 0; i < mc_ctrl.dir.file_num; i++)
    {
        mc_ctrl.dir_size += (mc_ctrl.dir.table[i].FileSizeByte + 1023) / 1024; // round up
    }

    // finally: add the size the dir itself takes by computing the number of clusters
    //          needed to memorize the files. since each cluster can hold 2 files,
    //          we need to divide the number by 2 and add 1 if the numer is odd.
    //          (thanks to @Mc-muffin)
    mc_ctrl.dir_size += mc_ctrl.dir.file_num / 2 + mc_ctrl.dir.file_num % 2;

    if (mc_ctrl.mode == MC_INIT || mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_SEL_LOAD)
    {
        file_num = 0;

        for (i = 0; i < mc_ctrl.dir.file_num; i++)
        {
            if (strindex((char *)mc_ctrl.dir.table[i].EntryName, KEYCODE PRODUCT_CODE "zero") >= 0)
            {
                file_num++;

                size = mc_ctrl.dir.table[i].FileSizeByte;

                if (!mcIsValidGameFileSize(size))
                {
                    return 2;
                }
            }
        }

        if (file_num != 3)
        {
            return 2;
        }
    }
    else
    {
        sprintf(str, "%s%d", KEYCODE PRODUCT_CODE "albm", mc_ctrl.sel_file);

        file_num = 0;

        for(i = 0; i < mc_ctrl.dir.file_num; i++)
        {
            if (strindex((char *)mc_ctrl.dir.table[i].EntryName, str) >= 0)
            {
                file_num++;

                size = mc_ctrl.dir.table[i].FileSizeByte;

                if (size != mc_album_size)
                {
                    return 1;
                }
            }
        }

        if (file_num != 1)
        {
            return 1;
        }
    }

    file_num = 0;

    for (i = 0; i < mc_ctrl.dir.file_num; i++)
    {
        if (strindex((char *)(mc_ctrl.dir.table[i].EntryName), "icon.sys") >= 0)
        {
            file_num++;

            size = mc_ctrl.dir.table[i].FileSizeByte;

            if (size != 0x3c4)
            {
                return 3;
            }
        }
    }

    if (file_num != 1)
    {
        return 3;
    }

    file_num = 0;

    for (i = 0; i < mc_ctrl.dir.file_num; i++)
    {
        if (strindex((char *)(mc_ctrl.dir.table[i].EntryName), (char *)mc_iconName[0]) >= 0)
        {
            file_num++;

            size = mc_ctrl.dir.table[i].FileSizeByte;

            if (mc_ctrl.mode == MC_INIT || mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_SEL_LOAD)
            {
                if (size != 0xf850)
                {
                    return 4;
                }
            }
            else
            {
                if (size != 0x104b0)
                {
                    return 4;
                }
            }
        }
    }

    if (file_num != 1)
    {
        return 4;
    }

    return 0;
}

char mcCheckRootList()
{
    u_int i;
    u_int j;
    u_int file_num;
    char str[256];

    for (i = 0; i < mc_ctrl.dir.file_num; i++)
    {
       // debug code ?
    }

    file_num = 0;

    for (i = 0; i < 5; i++)
    {
        mc_ctrl.album.file_no[i] = 0xff;
        mc_ctrl.album.sta[i] = 0xff;
    }

    for (i = 0; i < mc_ctrl.dir.file_num; i++)
    {
        if (strindex((char *)mc_ctrl.dir.table[i].EntryName, KEYCODE PRODUCT_CODE "albm") >= 0)
        {
            j = 0;

            while (mc_ctrl.dir.table[i].EntryName[j] != '\0')
            {
                j++;
            }

            mc_ctrl.album.file_no[file_num] = mc_ctrl.dir.table[i].EntryName[j-1] - '0';

            file_num++;
        }
    }

    mc_ctrl.album.file_num = file_num;

    return 0;
}

u_int mcGetFileSizeCeil(u_int file_id, u_int block)
{
    u_int size;

    size = mcGetFileSize(file_id);

    if (block != 0)
    {
        size += (block - size % block) % block;
    }

    return size;
}

u_int mcGetFileSize(u_int file_id)
{
    u_int size;
    int i;

    size = 0;

    if (file_id == 5 || file_id == 6 || file_id== 7)
    {
        for (i = 0; i < mc_gamedata_str_num; i++)
        {
            size += mc_gamedata_str[i].size;
        }
    }
    else
    {
        for (i = 0; i < mc_albumdata_str_num; i++)
        {
            size += mc_albumdata_str[i].size;
        }
    }

    return size + 16;
}

char mcCheckHeaderFile()
{
    u_int i;
    u_int file_num;

    file_num = 0;

    for (i = 0; i < 3; i++)
    {
        if (mc_header_buf[i].game.map_flg == 1)
        {
            file_num++;
        }
    }

    mc_ctrl.dir.gamefile_num = file_num;

    if (mc_ctrl.mode == 1)
    {
        if (mc_ctrl.dir.gamefile_num == 0)
        {
            return 1;
        }
    }

    return 0;
}

int strindex(char *s, char *t)
{
    int i;
    int j;
    int k;

    i = 0;

    while (s[i] != '\0')
    {
        j = i;
        k = 0;

        while (t[k] != '\0' && s[j] == t[k])
        {
            j++;
            k++;
        }

        if (k > 0 && t[k] == '\0')
        {
            return i;
        }

        i++;
    }

    return -1;
}

void mcMakeSaveFile(u_int *work_addr, u_char file_id)
{
    u_char *addr0;
    u_char *addr1;
    int i;
    int j;
    u_int sum;
    u_int size;
    u_int str_num;
    MC_DATA_STR *data_str;

    sum = 0;

    switch (file_id)
    {
    case 5:
    case 6:
    case 7:
        size = mc_game_size;
        str_num = mc_gamedata_str_num;
        data_str = mc_gamedata_str;
    break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        str_num = mc_albumdata_str_num;
        size = mc_album_size;

        if (save_pht_in_game != 0)
        {
            data_str = mc_albumdata2_str;
        }
        else
        {
            data_str = mc_albumdata_str;
        }
    break;
    default:
        return;
    break;
    }

    memset(work_addr, 0, size);

    addr0 = (u_char *)&work_addr[4];

    for (i = 0; i < str_num; i++)
    {
        addr1 = mcResolveDataAddress(&data_str[i]);

        for (j = 0; j < data_str[i].size; j++)
        {
            sum += *addr1;

            *addr0++ = *addr1++;
        }
    }

    *work_addr = sum;
}

char mcSetLoadFile(u_int *work_addr, u_char file_id)
{
    u_char *addr0;
    u_char *addr1;
    int i;
    int j;
    u_int size;
    u_int str_num;
    u_int legacy_size;
    MC_DATA_STR *data_str;

    switch (file_id)
    {
    case 5:
    case 6:
    case 7:
        size = mc_game_size;
        str_num = mc_gamedata_str_num;
        data_str = mc_gamedata_str;
    break;
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
        size = mc_album_size;
        str_num = mc_albumdata_str_num;
        data_str = mc_albumdata_str;
    break;
    default:
        return 0;
    break;
    }

    if (mc_ctrl.rw.header_flg != 0)
    {
        size = 0x200;
        str_num = 1;
    }
    else if ((file_id == 5 || file_id == 6 || file_id == 7)
             && mcIsValidGameFileSize(mc_ctrl.rw.size))
    {
        size = mc_ctrl.rw.size;
    }

    addr0 = (u_char *)&work_addr[0];

    if (mc_ctrl.rw.header_flg == 0)
    {
        if (mcCompareCheckSum(addr0, size) != 0)
        {
            return 1;
        }
    }

    addr0 = (u_char *)&work_addr[4];

    for (i = 0; i < str_num; i++)
    {
        if (size == mcGetLegacyGameFileSize()
            && data_str[i].addr_kind == MC_DATA_ADDR_HOST
            && data_str[i].addr == (uintptr_t)stage_wrk)
        {
            legacy_size = (u_int)(sizeof(MC_LEGACY_STAGE_WRK)
                          * (sizeof(stage_wrk) / sizeof(stage_wrk[0])));
            mcLoadLegacyStageWrk(addr0);
            addr0 += legacy_size;
            continue;
        }

        addr1 = mcResolveDataAddress(&data_str[i]);

        for (j = 0; j < data_str[i].size; j++)
        {
            *addr1++ = *addr0++;
        }
    }

    return 0;
}

char mcCompareCheckSum(u_char *data_ptr, int data_size)
{
    int sum;
    int data_sum;
    int i;

    data_sum = *(int *)data_ptr;

    data_size -= 16;
    data_ptr += 16;

    sum = 0;

    for (i = 0; i < data_size; i++)
    {
        sum += *data_ptr++;
    }

    return data_sum != sum;
}

void mcMakeHeaderFile()
{
    if (mc_ctrl.mode == MC_INIT || mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_SEL_LOAD)
    {
        MC_GAME_HEADER *header;

        header = &mc_header.game;

        header->map_flg = 1;

        if (ingame_wrk.mode == INGAME_MODE_ENDING)
        {
            header->msn_no = 5;
        }
        else
        {
            header->msn_no = ingame_wrk.msn_no;
        }

        header->room_no = plyr_wrk.pr_info.room_no;
        header->hour = time_wrk.game / (FRAMES_PER_SECOND * SECONDS_PER_HOUR);
        header->minute = (time_wrk.game / (FRAMES_PER_SECOND * SECONDS_PER_MINUTE)) % FRAMES_PER_SECOND;
        header->sec = (time_wrk.game / (FRAMES_PER_SECOND * 1)) % FRAMES_PER_SECOND;
        header->frame = time_wrk.game % FRAMES_PER_SECOND;
        header->msn_flg = mc_msn_flg;
#ifdef BUILD_EU_VERSION
        header->language = mc_language;
#endif

        if (ingame_wrk.clear_count == 0)
        {
            header->clear_flg = 0;
        }
        else
        {
            header->clear_flg = 1;
        }

        header->difficult = ingame_wrk.difficult;
    }
    else
    {
        MC_ALBUM_HEADER *header;

        header = &mc_header.album;

        header->map_flg = 1;
        header->photo_num = mc_photo_num;
        header->type = mc_ctrl.album.type;
        header->pad[0] = 0xff;
        header->pad[1] = 0xff;
        header->pad[2] = 0xff;
        header->pad[3] = 0xff;
        header->pad[4] = 0xff;
    }
}

void mcSetHeaderFile()
{
    if (mc_ctrl.mode == MC_INIT || mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_SEL_LOAD)
    {
        MC_GAME_HEADER *header;

        header = &mc_header.game;

        if (mc_header.game.msn_no == 5)
        {
            ingame_wrk.msn_no = 0;
        }
        else
        {
            ingame_wrk.msn_no = header->msn_no;
        }

        plyr_wrk.pr_info.room_no = header->room_no;

        time_wrk.game = header->hour * FRAMES_PER_SECOND * SECONDS_PER_HOUR +
            header->minute * FRAMES_PER_SECOND * SECONDS_PER_MINUTE +
            header->sec * FRAMES_PER_SECOND + header->frame;

        mc_msn_flg = header->msn_flg;
    }
    else
    {
        MC_ALBUM_HEADER *header;

        header = &mc_header.album;

        mc_photo_num = header->photo_num;
        mc_album_type = header->type;
    }
}

char mcGetAlbumNo()
{
    u_int i;
    char flg[5] = { 0, 0, 0, 0, 0 };

    for (i = 0; i < mc_ctrl.album.file_num; i++)
    {
        flg[mc_ctrl.album.file_no[i]] = 1;
    }

    for (i = 0; i < 5; i++)
    {
        if (flg[i] == '\0')
        {
            return i;
        }
    }

    return -1;
}

void mcSetIconSys(sceMcIconSys *pSys)
{
    u_int i;
    char str[256];

    *pSys = {};

    memcpy(pSys->Head, "PS2D", sizeof(pSys->Head)+1);

#ifdef BUILD_EU_VERSION
    pSys->OffsLF = 22;
#else
    pSys->OffsLF = 20;
#endif
    pSys->TransRate = 96;

    memcpy(pSys->BgColor, mc_bgcolor, sizeof(mc_bgcolor));
    memcpy(pSys->LightDir, mc_lightdir, sizeof(mc_lightdir));
    memcpy(pSys->LightColor, mc_lightcol, sizeof(mc_lightcol));
    memcpy(pSys->Ambient, mc_ambient, sizeof(mc_ambient));

    strcpy((char *)pSys->FnameView, (char *)mc_iconName[0]);
    strcpy((char *)pSys->FnameCopy, (char *)mc_iconName[1]);
    strcpy((char *)pSys->FnameDel, (char *)mc_iconName[2]);

    if (mc_ctrl.mode == MC_INIT || mc_ctrl.mode == MC_CHECK || mc_ctrl.mode == MC_SEL_LOAD)
    {
        strcpy((char *)pSys->TitleName, mc_icon_title);
    }
    else
    {
        i = 0;

        strcpy(str, mc_icon_album);

        while (str[i] != '\0')
        {
            i++;
        }

        str[i-1] += (char)mc_ctrl.sel_file;

        strcpy((char *)pSys->TitleName, str);
    }
}

u_char save_pht_in_game = 0;
