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

/// ─────────────────────────── GPU skinning bridge ───────────────────────────
/// When GPU skinning is enabled, SetVUVNDataPost gathers the static bind-pose
/// data and the per-frame bone-matrix palette instead of running the CPU skin
/// (calc_skinned_data). The mesh-0x2/0xA renderer reads these to upload the
/// bind data once (cached) and the palette per frame, doing the blend in the VS.

/// Runtime toggle. Default on. When off, the legacy CPU-skin + per-frame vertex
/// stream path is used and the gather/accessors below report "no skinning".
int  MikuPan_GpuSkinningEnabled(void);
void MikuPan_SetGpuSkinningEnabled(int enabled);

/// Skin mode recorded for the most recently processed mesh:
///   0 = not GPU-skinned (renderer uses the legacy streamed path)
///   2 = vtype 2 (one bone-pair for the whole mesh)
///   3 = vtype 3 (per-vertex bone-pair)
int MikuPan_SkinMode(void);

/// Vertex count and bind-pose staging for the current skinned mesh. The staging
/// holds `count` vertices, 64 bytes each: four float4 = {bone0 pos+weight,
/// bone1 pos+packed-indices, bone0 normal+normal-weight, bone1 normal}.
int          MikuPan_SkinVertexCount(void);
const void  *MikuPan_SkinBindData(void);

/// Bone-matrix palette for the current skinned mesh: `*out_bones` matrices,
/// 64 bytes each (sceVu0FMATRIX rows). Stable across all meshes of one model in
/// a frame, so the renderer hash-guards the upload.
const void  *MikuPan_SkinPalette(int *out_bones);

/// Content hash of the current palette (computed once per model per pass). The
/// renderer uses it to upload the storage buffer only when the bytes change —
/// which also dedupes the scene + mirror-reflection passes (identical content).
unsigned long long MikuPan_SkinPaletteHash(void);

/// Reset per-frame palette dedupe state. Call once at the start of each frame
/// so the first skinned mesh of every model re-reads its animated pose.
void MikuPan_SkinFrameReset(void);

#endif // GRAPHICS_GRAPH3D_SGSU_H
