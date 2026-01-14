#ifndef MIKUPAN_TYPES_H
#define MIKUPAN_TYPES_H

#define GET_NUM_MESH(intpointer) (char)((char*)intpointer)[14]
#include "typedefs.h"

typedef float LMATRIX[3][4];
typedef float VECTOR3[3];
typedef unsigned int void_type[4];

struct _LIGHT_POINT_AND_SPOT_NUM
{
  int iPad0;
  int iNumSpotGroup;
  int iNumPointGroup;
  int iPad1;
};

union SCEGIFTAG_EOP
{
  qword gtEOP;
  struct _LIGHT_POINT_AND_SPOT_NUM lightnum;
};

typedef enum {
  LID_DIRECTIONAL_0 = 0,
  LID_DIRECTIONAL_1 = 1,
  LID_DIRECTIONAL_2 = 2,
  LID_POINT_0 = 3,
  LID_POINT_1 = 4,
  LID_POINT_2 = 5,
  LID_POINT_3 = 6,
  LID_POINT_4 = 7,
  LID_POINT_5 = 8,
  LID_POINT_6 = 9,
  LID_POINT_7 = 10,
  LID_POINT_8 = 11,
  LID_POINT_9 = 12,
  LID_POINT_10 = 13,
  LID_POINT_11 = 14,
  LID_POINT_12 = 15,
  LID_POINT_13 = 16,
  LID_POINT_14 = 17,
  LID_POINT_15 = 18,
  LID_POINT_FLASHLIGHT_0 = 19,
  LID_POINT_FLASHLIGHT_1 = 20,
  LID_POINT_SELFREFLECTION = 21,
  LID_SPOT_0 = 22,
  LID_SPOT_1 = 23,
  LID_SPOT_2 = 24,
  LID_SPOT_3 = 25,
  LID_SPOT_4 = 26,
  LID_SPOT_5 = 27,
  LID_SPOT_6 = 28,
  LID_SPOT_7 = 29,
  LID_SPOT_8 = 30,
  LID_SPOT_9 = 31,
  LID_SPOT_10 = 32,
  LID_SPOT_11 = 33,
  LID_SPOT_12 = 34,
  LID_SPOT_13 = 35,
  LID_SPOT_14 = 36,
  LID_SPOT_15 = 37,
  LID_SPOT_FLASHLIGHT = 38,
  NUM_GRA3DLIGHTID = 39,
  GRA3D_START_LIGHT_DIRECTIONAL = 0,
  GRA3D_END_LIGHT_DIRECTIONAL = 2,
  GRA3D_NUM_LIGHT_DIRECTIONAL = 3,
  GRA3D_START_LIGHT_POINT = 3,
  GRA3D_END_LIGHT_POINT = 21,
  GRA3D_NUM_LIGHT_POINT = 19,
  GRA3D_NUM_LIGHT_POINT_STATIC = 16,
  GRA3D_START_LIGHT_SPOT = 22,
  GRA3D_END_LIGHT_SPOT = 38,
  GRA3D_NUM_LIGHT_SPOT = 17,
  GRA3D_NUM_LIGHT_SPOT_STATIC = 16,
  INVALID_GRA3DLIGHTID = 2147483647,
  GRA3DLIGHTID_FORCE_DWORD = 2147483647
} GRA3DLIGHTID;

typedef enum {
  G3DLIGHT_DIRECTIONAL = 0,
  G3DLIGHT_POINT = 1,
  G3DLIGHT_SPOT = 2,
  NUM_G3DLIGHTTYPE = 3,
  G3DLIGHT_AMBIENT = 3,
  INVALID_G3DLIGHTTYPE = 2147483647,
  G3DLIGHTTYPE_FORCE_DWORD = 2147483647
} G3DLIGHTTYPE;

struct G3DLIGHT {
  /* 0x00 */ sceVu0FVECTOR vDiffuse;
  /* 0x10 */ sceVu0FVECTOR vSpecular;
  /* 0x20 */ sceVu0FVECTOR vAmbient;
  /* 0x30 */ sceVu0FVECTOR vPosition;
  /* 0x40 */ sceVu0FVECTOR vDirection;
  /* 0x50 */ G3DLIGHTTYPE Type;
  /* 0x54 */ float fAngleInside;
  /* 0x58 */ float fAngleOutside;
  /* 0x5c */ float fMaxRange;
  /* 0x60 */ float fMinRange;
  /* 0x64 */ float fFalloff;
  /* 0x68 */ float afPad0[2];
};

struct G3DVIF1CODE_UNPACK { // 0x4
  /* 0x0:0 */ unsigned int ADDR: 10;
  /* 0x1:2 */ unsigned int pad: 4;
  /* 0x1:6 */ unsigned int USN: 1;
  /* 0x1:7 */ unsigned int FLG: 1;
  /* 0x2:0 */ unsigned int NUM: 8;
  /* 0x3:0 */ unsigned int CMD: 8;
};

struct GRA3DMATERIALINDEXCACHE {
  /* 0x0 */ int bEnable;
  /* 0x4 */ int aiIndex[3];
};

struct GRA3DVU1LIGHTDATA_DIRECTIONAL
{
  LMATRIX lmDiffuse;
  LMATRIX lmSpecular;
};

struct GRA3DVU1LIGHTDATA_POINT
{
  LMATRIX lmPosition;
  float _vDirectionInverse[4];
};

struct GRA3DVU1LIGHTDATA_SPOT
{
  LMATRIX lmPosition;
  float vIntens[4];
  float vIntensB[4];
  LMATRIX lmDirection;
  LMATRIX lmSpecular;
};

struct GRA3DVU1LIGHTDATA
{
  struct GRA3DVU1LIGHTDATA_DIRECTIONAL dir;
  struct GRA3DVU1LIGHTDATA_SPOT spot;
  struct GRA3DVU1LIGHTDATA_POINT point;
};

struct GRA3DVU1DBADDRESS
{
  unsigned int uiContext0;
  unsigned int uiContext1;
  unsigned int auiPad[2];
};

struct G3DVU1FOG
{
  float fMin;
  float fMax;
  float FA;
  float FB;
};

struct GRA3DVU1MEMLAYOUT_DIRECT
{
  float _vVF1[4];
  float _vVF2[4];
  struct GRA3DVU1DBADDRESS DBAddress;
  qword gtTRISTRIP_NOTEXTURE;
  qword gtTRISTRIP_TEXTURE;
  union SCEGIFTAG_EOP gtEOP;
  qword gtTRIFAN_NOTEXTURE;
  qword gtTRIFAN_TEXTURE;
  float matWorldScreen[4][4];
  float matWorldClip[4][4];
  float matLocalWorld[4][4];
  float matLocalWorldNoNormalized[4][4];
  struct G3DVU1FOG Fog;
  LMATRIX lmatDiffuse_Directional;
  LMATRIX lmatSpecular_Directional;
  float vPosition_Spot0[4];
  float vPosition_Spot1[4];
  float vPosition_Spot2[4];
  float vIntension_Spot[4];
  float vIntensionB_Spot[4];
  LMATRIX lmatDiffuse_Spot;
  LMATRIX lmatSpecular_Spot;
  float vPosition_Point0[4];
  float vPosition_Point1[4];
  float vPosition_Point2[4];
  float _vEyeDirection_Point[4];
  float vAmbient_Directional[4];
  LMATRIX lmatDiffuse_Directional_Material;
  LMATRIX lmatSpecular_Directional_Material;
  float vBTimes_Spot[4];
  LMATRIX lmatDiffuse_Spot_Material;
  LMATRIX lmatSpecular_Spot_Material;
  float vBTimes_Point[4];
  LMATRIX lmatDiffuse_Point_Material;
  LMATRIX lmatSpecular_Point_Material;
};

struct GRA3DVU1CONSTDATA
{
  float _vVF1[4];
  float _vVF2[4];
  struct GRA3DVU1DBADDRESS DBAddress;
  qword gtTRISTRIP_NOTEXTURE;
  qword gtTRISTRIP_TEXTURE;
  union SCEGIFTAG_EOP gtEOP;
  qword gtTRIFAN_NOTEXTURE;
  qword gtTRIFAN_TEXTURE;
};

struct GRA3DVU1TRANSFORMDATA
{
  float matWorldScreen[4][4];
  float matWorldClip[4][4];
  float matLocalWorld[4][4];
  float matLocalWorldNoNormalized[4][4];
};

struct GRA3DVU1MATERIALDATA_POINT
{
  float vPower[4];
  LMATRIX lmDiffuse;
  LMATRIX lmSpecular;
};

struct GRA3DVU1MATERIALDATA_SPOT
{
  float vPower[4];
  LMATRIX lmDiffuse;
  LMATRIX lmSpecular;
};

struct GRA3DVU1MATERIALDATA_DIRECTIONAL
{
  float vAmbient[4];
  LMATRIX lmDiffuse;
  LMATRIX lmSpecular;
};

struct GRA3DVU1MATERIALCACHE_POINT
{
  struct GRA3DVU1MATERIALDATA_POINT Data;
  struct GRA3DMATERIALINDEXCACHE Index;
};

struct GRA3DVU1MATERIALCACHE_SPOT
{
  struct GRA3DVU1MATERIALDATA_SPOT Data;
  struct GRA3DMATERIALINDEXCACHE Index;
};

struct GRA3DVU1MATERIALDATA
{
  struct GRA3DVU1MATERIALDATA_DIRECTIONAL dir;
  struct GRA3DVU1MATERIALDATA_SPOT spot;
  struct GRA3DVU1MATERIALDATA_POINT point;
};

struct GRA3DVU1MEMLAYOUT_PACKED
{
  struct GRA3DVU1CONSTDATA Const;
  struct GRA3DVU1TRANSFORMDATA Transform;
  struct G3DVU1FOG Fog;
  struct GRA3DVU1LIGHTDATA Light;
  struct GRA3DVU1MATERIALDATA Material;
};

union GRA3DVU1MEMLAYOUT
{
  struct GRA3DVU1MEMLAYOUT_DIRECT Direct;
  struct GRA3DVU1MEMLAYOUT_PACKED Packed;
};

struct GRA3DSCRATCHPADLAYOUT
{
  qword qwVif1Code0;
  union GRA3DVU1MEMLAYOUT Vu1Mem;
};

struct SGDVUMESHPOINTNUM
{
    struct G3DVIF1CODE_UNPACK VifUnpack;
    unsigned int uiPointNum;
};

struct SGDMESHVERTEXDATA_TYPE2 { // 0x18
    /* 0x00 */ float vVertex[3];
    /* 0x0c */ float vNormal[3];
};

struct SGDVUVNDESC { // 0x8
    /* 0x0 */ short int sNumVertex;
    /* 0x2 */ short int sNumNormal;
    /* 0x4 */ unsigned char ucSize;
    /* 0x5 */ unsigned char ucVectorType;
    /* 0x6 */ unsigned char aucPad[2];
};

struct SGDVUVNDATA_PRESET { // 0x40
    /* 0x00 */ unsigned int aui[10];
    /* 0x28 */ union { // 0x18
        /* 0x28 */ struct SGDMESHVERTEXDATA_TYPE2 avt2[1];
    };
};

struct _SGDVUMESHCOLORDATA { // 0x10
    /* 0x0 */ struct G3DVIF1CODE_UNPACK VifUnpack;
    /* 0x4 */ VECTOR3 avColor[1];
};

struct SGDVUMESHDESC { // 0x8
    /* 0x0 */ int iTagSize;
    /* 0x4 */ unsigned char ucPad0;
    /* 0x5 */ union { // 0x1
        /* 0x5 */ unsigned char ucMeshType;
    };
    /* 0x6 */ unsigned char ucNumMesh;
    /* 0x7 */ unsigned char ucPad1;
};

struct SGDPROCUNITHEADER { // 0x10
    /* 0x0 */ int pNext;
    /* 0x4 */ int iCategory;
    /* 0x8 */ union { // 0x8
        /* 0x8 */ long int lPrimType;
        /* 0x8 */ struct SGDVUVNDESC VUVNDesc;
        /* 0x8 */ struct SGDVUMESHDESC VUMeshDesc;
    };
};

struct SGDVUVNDATA {
    /* 0x00 */ qword qwVif1Code;
    /* 0x10 */ unsigned int uiVTop;
    /* 0x14 */ unsigned int uiPTop;
    /* 0x18 */ unsigned int uiWTop;
    /* 0x1c */ unsigned int uiNumMesh;
};

struct SGDVUMESHDATA { // 0x20
    /* 0x00 */ u_int qwVif1Code[4];
    /* 0x10 */ sceGifTag GifTag;
};

struct SGDVUMESHDATA_PRESET { // 0x18
    /* 0x00 */ short int asPad0[2];
    /* 0x04 */ short int sOffsetToST;
    /* 0x06 */ short int sOffsetToPrim;
    /* 0x08 */ int aiPad1[2];
    /* 0x10 */ long int alData[1];
};

struct SGDLIGHTDATA_DIRECTIONAL { // 0x20
    /* 0x00 */ float vColor[4];
    /* 0x10 */ float vDirection[4];
};

struct SGDLIGHTDATA_POINT { // 0x20
    /* 0x00 */ float vColor[4];
    /* 0x10 */ float vPosition[4];
};

struct SGDLIGHTDATA_SPOT { // 0x30
    /* 0x00 */ float vColor[4];
    /* 0x10 */ float vPosition[4];
    /* 0x20 */ float vTarget[4];
};

struct SGDLIGHTDATA_AMBIENT { // 0x10
    /* 0x0 */ float vColor[4];
};

struct SGDTEXTUREANIMATIONDESC { // 0x8
    /* 0x0 */ unsigned char ucNumTexture;
    /* 0x1 */ unsigned char ucPaddingSize;
    /* 0x2 */ unsigned char bEnable;
    /* 0x3 */ unsigned char ucStep;
    /* 0x4 */ unsigned char ucCount;
    /* 0x5 */ unsigned char bLoop;
    /* 0x6 */ unsigned char aucPad[2];
};

struct SGDGSIMAGEDATA { // 0x30
    /* 0x00 */ unsigned int auiVifCode[4];
    /* 0x10 */ void_type GT;
    /* 0x20 */ unsigned char aucData[1];
};

struct SGDVUMESHST
{
    float fS;
    float fT;
};

struct SGDVUMESHSTDATA
{
    struct G3DVIF1CODE_UNPACK VifUnpack;
    struct SGDVUMESHST astData[1];
};

union SGDPROCUNITDATA { // 0x80
    /* 0x00 */ struct SGDVUVNDATA VUVNData;
    /* 0x00 */ struct SGDVUMESHDATA VUMeshData;
    /* 0x00 */ struct SGDVUVNDATA_PRESET VUVNData_Preset;
    /* 0x00 */ struct SGDVUMESHDATA_PRESET VUMeshData_Preset;
    /* 0x00 */ unsigned char aucGSImage;
    /* 0x00 */ float avBB[8][4];
    /* 0x00 */ struct SGDLIGHTDATA_DIRECTIONAL alightDirectional[1];
    /* 0x00 */ struct SGDLIGHTDATA_POINT alightPoint[1];
    /* 0x00 */ struct SGDLIGHTDATA_SPOT alightSpot[1];
    /* 0x00 */ struct SGDLIGHTDATA_AMBIENT lightAmbient;
    /* 0x00 */ struct SGDGSIMAGEDATA GSImage;
};

struct G3DVIF1CODE_DIRECT {
    unsigned int size : 16;
    unsigned int num : 8;
    unsigned int cmd : 7;
    unsigned int irq : 1;
};

typedef struct
{
    unsigned int uiVif1Code_NOP0;
    unsigned int uiVif1Code_NOP1;
    unsigned int uiVif1Code_FLUSH;
    struct G3DVIF1CODE_DIRECT uiVif1Code_DIRECT;
    sceGsLoadImage gsli;
} SGDTRI2FILEHEADER;

#endif //MIKUPAN_TYPES_H
