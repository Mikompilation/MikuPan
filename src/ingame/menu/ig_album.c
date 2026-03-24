#include "common.h"

#include "ig_album.h"

void NewgameMenuAlbumInit()
{
}

void LoadgameMenuAlbumInit()
{
}

void StartAlbumModeInit()
{
}

void IngameMenuAlbum(char is_outgame)
{
}

void IngameAlbumAllPad(u_char* csr, u_char* csr_top, u_char* mode)
{
}

void IngameAlbumLstPad(u_char* csr, u_char* csr_top, u_char* mode)
{
}

void IngameAlbumBigPad(u_char* csr, u_char* csr_top, u_char* mode)
{
}

void IngameAlbumDispPhotoLarge(u_char csr)
{
}

void IngameAlbumDisp(u_char csr, u_char csr_top, u_char mode, char is_outgame)
{
}

u_char* GetSubjectNameAddr(u_short kind, u_short no, u_short show)
{
    char* aa = "jebfgksjhbkjlse";
    return aa;
}

u_char TimeIsMoney(u_char hexhex)
{
    return 0;
}

u_char MoneyIsTime(u_char time)
{
    return 0;
}

void PutDate(PICTURE_WRK *pic_inf, short int pos_x, short int pos_y,
             short int alpha, int pri)
{
}

void OutGameInitPhoto()
{
}

int isPhotoEnd()
{
    return 1;
}

float GetPhtDispAlpha()
{
    return 0.5f;
}
