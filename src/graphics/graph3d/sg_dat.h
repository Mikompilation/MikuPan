#ifndef GRAPHICS_GRAPH3D_SG_DAT_H
#define GRAPHICS_GRAPH3D_SG_DAT_H

#include "typedefs.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
	int num;
	int lnum[3];
} MatCache;

typedef struct {
	u_int HeaderSections;
	u_int UniqHeaderSize;
	//sceVu0FVECTOR *pUniqVertex;
	int pUniqVertex;
	//sceVu0FVECTOR *pUniqNormal;
	int pUniqNormal;
	//u_int *pUniqList;
	u_int pUniqList;
	u_int CommonHeaderSize;
	//sceVu0FVECTOR *pCommonVertex;
	int pCommonVertex;
	//sceVu0FVECTOR *pCommonNormal;
	int pCommonNormal;
	//u_int *pCommonList;
	int pCommonList;
	u_int WeightedHeaderSize;
	//sceVu0FVECTOR *pWeightedVertex;
	int pWeightedVertex;
	//sceVu0FVECTOR *pWeightedNormal;
	int pWeightedNormal;
	//u_int *pWeightedList;
	int pWeightedList;
} PHEAD;

typedef struct SgCOORDUNIT SgCOORDUNIT;

struct SgCOORDUNIT {
	sceVu0FMATRIX matrix;
	sceVu0FMATRIX lwmtx;
	sceVu0FMATRIX workm;
	sceVu0FVECTOR rot;
	//SgCOORDUNIT *parent;
	int parent;
	u_int flg;
	u_int edge_check;
	u_int camin;
};

typedef struct {
	sceVu0FVECTOR pos;
	sceVu0FVECTOR direction;
	sceVu0FVECTOR ldirection;
	sceVu0FVECTOR spvector;
	sceVu0FVECTOR diffuse;
	sceVu0FVECTOR specular;
	sceVu0FVECTOR ambient;
	float intens;
	float intens_b;
	float power;
	float btimes;
	float spower;
	int Enable;
	int SEnable;
	int num;
} SgLIGHT;

typedef struct {
	short int cn0;
	short int cn1;
	u_short vIdx;
	u_short vOff;
} ONELIST;

typedef struct {
	int list_num;
	ONELIST lists[1];
} VERTEXLIST;

typedef struct {
	u_int primtype;
	char name[12];
	sceVu0FVECTOR Ambient;
	sceVu0FVECTOR Diffuse;
	sceVu0FVECTOR Specular;
	sceVu0FVECTOR Emission;
	u_int vifcode[4];
	u_long128 giftag;
	u_int Tex0[4];
	u_int Tex1[4];
	u_int Miptbp1[4];
	u_int Miptbp2[4];
} SgMaterial;

typedef struct { // 0x90
	/* 0x00 */ u_int primtype;
	/* 0x04 */ char name[12];
	/* 0x10 */ sceVu0FVECTOR Ambient;
	/* 0x20 */ sceVu0FVECTOR Diffuse;
	/* 0x30 */ sceVu0FVECTOR Specular;
	/* 0x40 */ sceVu0FVECTOR Emission;
	/* 0x50 */ int cache_on;
	/* 0x54 */ u_int tagd_addr;
	/* 0x58 */ int qwc;
	/* 0x5c */ MatCache Parallel;
	/* 0x6c */ MatCache Point;
	/* 0x7c */ MatCache Spot;
} SgMaterialC;

typedef struct {
	u_int VersionID;
	u_char MAPFLAG;
	u_char kind;
	u_short materials;
	//SgCOORDUNIT *coordp;
	int coordp;
	//SgMaterial *matp;
	int matp;
	//u_int *phead;

	/// pVectorInfo
	u_int phead;
	u_int blocks;
	u_int primitives[];
} HeaderSection;

inline static u_int * GetPHead(HeaderSection *hs)
{
	return (u_int *)((u_int)hs->phead + (int64_t)hs);
}

inline static void* GetOffsetPtr(void *p, int offset)
{
	return (u_int *)((u_int)offset + (int64_t)p);
}

inline static SgCOORDUNIT* GetCoordP(HeaderSection *hs)
{
    return (SgCOORDUNIT *)((int)hs->coordp + (int64_t)hs);
}

inline static SgCOORDUNIT* GetCoordPParent(SgCOORDUNIT* cp)
{
    return (SgCOORDUNIT *)((int)cp->parent + (int64_t)cp);
}

inline static SgMaterial* GetMatP(HeaderSection *hs)
{
    return (SgMaterial *)((int)hs->matp + (int64_t)hs);
}

inline static SgMaterial* GetMaterialPtr(const HeaderSection *hs, int index)
{
	if (index > hs->materials)
	{
		return NULL;
	}

	SgMaterial* pMaterial = (SgMaterial *) ((int64_t) hs + (int) hs->matp);
	return &pMaterial[index];
}

inline static u_int* GetTopProcUnitHeaderPtr(HeaderSection *hs, int index)
{
	if (hs->primitives[index] != 0x0)
	{
		return (u_int*) ((int64_t) hs
					      + (unsigned int) hs->primitives[index]);
	}

	return NULL;
}

static inline u_int* GetNextProcUnitHeaderPtr(const u_int *pHead)
{
	if (pHead[0] != 0x0)
	{
		return (u_int *) ((int64_t) pHead + pHead[0]);
	}

	return NULL;
}

typedef struct {
    short int vnum;
    short int nnum;
    char vif_size;
    char vtype;
} VUVN_PRIM;

#endif // GRAPHICS_GRAPH3D_SG_DAT_H
