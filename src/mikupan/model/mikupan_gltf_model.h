#ifndef MIKUPAN_GLTF_MODEL_H
#define MIKUPAN_GLTF_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MikuPan_GltfModel MikuPan_GltfModel;

typedef struct MikuPan_GltfModelInfo
{
    int mesh_count;
    int material_count;
    int texture_count;
    int vertex_count;
    int index_count;
    int skinned_mesh_count;
    int bone_count;
    int node_mapped_mesh_count;
    int node_unmapped_mesh_count;
} MikuPan_GltfModelInfo;

MikuPan_GltfModel *MikuPan_LoadGltfModel(const char *path);
void MikuPan_FreeGltfModel(MikuPan_GltfModel *model);
void MikuPan_RenderGltfModel(const MikuPan_GltfModel *model);
void MikuPan_RenderGltfModelWithNodeMatrices(const MikuPan_GltfModel *model,
                                             const float *matrices,
                                             int matrix_count);
int MikuPan_GetGltfModelInfo(const MikuPan_GltfModel *model,
                             MikuPan_GltfModelInfo *out_info);

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_GLTF_MODEL_H
