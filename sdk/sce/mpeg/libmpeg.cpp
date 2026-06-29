#include "libmpeg.h"

int sceMpegReset(sceMpeg* mp)
{
    return 0;
}

int sceMpegCreate(sceMpeg* mp, u_char* work_area, int work_area_size)
{
    return 0;
}

int sceMpegDelete(sceMpeg* mp)
{
    return 0;
}

int sceMpegGetPicture(sceMpeg* mp, sceIpuRGB32* rgb32, int mbcount)
{
    return 0;
}

int sceMpegIsEnd(sceMpeg* mp)
{
    return 0;
}

int sceMpegIsRefBuffEmpty(sceMpeg* mp)
{
    return 0;
}

void sceMpegSetDecodeMode(sceMpeg* mp, int ni, int np, int nb)
{
}

sceMpegCallback sceMpegAddCallback(sceMpeg* mp, sceMpegCbType type, sceMpegCallback callback, void* anyData)
{
    return callback;
}

sceMpegCallback sceMpegAddStrCallback(sceMpeg* mp, sceMpegStrType strType, int ch, sceMpegCallback callback,
    void* anyData)
{
    return callback;
}
