#ifndef MIKUPAN_MIKUPAN_BASICTYPES_H
#define MIKUPAN_MIKUPAN_BASICTYPES_H
#include "typedefs.h"

typedef struct
{
    float x;
    float y;
    float w;
    float h;
} MikuPan_Rect;

typedef struct
{
    int width;
    int height;
    u_int id;
    unsigned char* data;
} MikuPan_TextureInfo;

#endif//MIKUPAN_MIKUPAN_BASICTYPES_H
