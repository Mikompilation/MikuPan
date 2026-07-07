#ifndef GRAPHICS_GRAPH3D_SGSU_H
#define GRAPHICS_GRAPH3D_SGSU_H

#include "typedefs.h"

#include "ee/eestruct.h"

#include "graphics/graph3d/sg_dat.h"

extern SgCOORDUNIT *lcp;
extern PHEAD *lphead;
extern u_int nextprim;
extern u_int *vuvnprim;
extern int blocksm;
extern int write_coord;

void _AddColor(float *v);
void SgSuDebugOn();
void SgSuDebugOff();
void SetDebugSign(int num);
void PutDebugSign();
void SgSetVNBuffer(sceVu0FVECTOR *vnarray, int size);
int CheckCoordCache(int cn);
void SetVU1Header();
void CalcVertexBuffer(u_int *prim);
u_int* SetVUVNData(u_int *prim);
u_int* SetVUVNDataPost(u_int *prim, int allow_gpu_skin);
void printTEX0(sceGsTex0 *tex0);
void SetVUMeshData(u_int *prim);
void SetVUMeshDataPost(u_int *prim);
void SetCoordData(u_int *prim);
void SgSortUnitPrim(u_int *prim);
void SgSortUnitPrimPost(u_int *prim);
void SgSortPreProcess(u_int *prim);
void AppendVUProgTag(u_int *prog);
void LoadSgProg(int load_prog);
void SetUpSortUnit();
void SgSortUnit(void *sgd_top, int pnum);
void SgSortUnitKind(void *sgd_top, int num);
void _SetLWMatrix0(sceVu0FMATRIX m0);
void _SetLWMatrix1(sceVu0FMATRIX m0);
void _SetRotTransPersMatrix(sceVu0FMATRIX m0);
void _CalcVertex(sceVu0FVECTOR *dp, float *v, float *n);
void _vfito0(int *v0);

int  MikuPan_GpuSkinningEnabled(void);
void MikuPan_SetGpuSkinningEnabled(int enabled);
int  MikuPan_GpuWeightedSkinningEnabled(void);
void MikuPan_SetGpuWeightedSkinningEnabled(int enabled);
int  MikuPan_SkinMode(void);
void MikuPan_ClearSkinMode(void);
int          MikuPan_SkinVertexCount(void);
const void  *MikuPan_SkinBindData(void);
const void  *MikuPan_SkinPalette(int *out_bones);
unsigned long long MikuPan_SkinPaletteHash(void);
void MikuPan_SkinFrameReset(void);

#endif // GRAPHICS_GRAPH3D_SGSU_H
