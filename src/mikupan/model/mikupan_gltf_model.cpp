#include "mikupan_gltf_model.h"

#include "mikupan/debug/mikupan_logging.h"
#include "mikupan/mikupan_basictypes.h"
#include "mikupan/rendering/mikupan_gl_compat.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_pipeline.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_renderer_internal.h"
#include "mikupan/rendering/mikupan_shader.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "sce/libvu0.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <array>
#include <climits>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

typedef struct MikuPanGltfPositionNormal
{
    float position[3];
    float normal[3];
} MikuPanGltfPositionNormal;

typedef struct MikuPanGltfSkinVertex
{
    float joints[4];
    float weights[4];
} MikuPanGltfSkinVertex;

typedef struct MikuPanGltfBone
{
    int node_index;
    float inverse_bind_matrix[16];
    float bind_global_matrix[16];
} MikuPanGltfBone;

typedef struct MikuPanGltfMesh
{
    unsigned int vertex_buffer;
    unsigned int uv_buffer;
    unsigned int color_buffer;
    unsigned int skin_buffer;
    unsigned int index_buffer;
    unsigned int vao;
    int index_count;
    int vertex_count;
    int material_index;
    int node_index;
    int skinned;
    int skin_palette_offset;
    float bind_matrix[16];
    float node_offset_matrix[16];
    std::vector<MikuPanGltfBone> bones;
} MikuPanGltfMesh;

typedef struct MikuPanGltfMaterial
{
    MikuPan_TextureInfo texture;
    int texture_valid;
    float base_color[4];
    std::string texture_path;
} MikuPanGltfMaterial;

struct MikuPan_GltfModel
{
    std::filesystem::path path;
    std::vector<MikuPanGltfMesh> meshes;
    std::vector<MikuPanGltfMaterial> materials;
    unsigned int skin_palette_buffer = 0;
    int skin_palette_joint_count = 0;
    mutable unsigned long long skin_palette_uploaded_hash = 0;
    mutable int skin_palette_uploaded_valid = 0;
    int texture_count = 0;
    int vertex_count = 0;
    int index_count = 0;
    int skinned_mesh_count = 0;
    int bone_count = 0;
    int node_mapped_mesh_count = 0;
    int node_unmapped_mesh_count = 0;
};

static void MikuPan_GltfSetIdentityMatrix(float matrix[16])
{
    memset(matrix, 0, sizeof(float) * 16u);
    matrix[0] = 1.0f;
    matrix[5] = 1.0f;
    matrix[10] = 1.0f;
    matrix[15] = 1.0f;
}

static void MikuPan_GltfCopyAiMatrix(float matrix[16], const aiMatrix4x4 &src)
{
    matrix[0] = src.a1;
    matrix[1] = src.b1;
    matrix[2] = src.c1;
    matrix[3] = src.d1;
    matrix[4] = src.a2;
    matrix[5] = src.b2;
    matrix[6] = src.c2;
    matrix[7] = src.d2;
    matrix[8] = src.a3;
    matrix[9] = src.b3;
    matrix[10] = src.c3;
    matrix[11] = src.d3;
    matrix[12] = src.a4;
    matrix[13] = src.b4;
    matrix[14] = src.c4;
    matrix[15] = src.d4;
}

static void MikuPan_GltfCopyAiRelativeMatrix(float matrix[16],
                                             const aiMatrix4x4 &base,
                                             const aiMatrix4x4 &global)
{
    sceVu0FMATRIX base_matrix;
    sceVu0FMATRIX global_matrix;
    sceVu0FMATRIX inverse_base_matrix;
    sceVu0FMATRIX relative_matrix;

    MikuPan_GltfCopyAiMatrix((float *)base_matrix, base);
    MikuPan_GltfCopyAiMatrix((float *)global_matrix, global);
    sceVu0InversMatrix(inverse_base_matrix, base_matrix);
    sceVu0MulMatrix(relative_matrix, inverse_base_matrix, global_matrix);
    memcpy(matrix, relative_matrix, sizeof(relative_matrix));
}

static aiMatrix4x4 MikuPan_GltfMakeAiIdentityMatrix(void)
{
    aiMatrix4x4 matrix;
    matrix.a1 = 1.0f;
    matrix.a2 = 0.0f;
    matrix.a3 = 0.0f;
    matrix.a4 = 0.0f;
    matrix.b1 = 0.0f;
    matrix.b2 = 1.0f;
    matrix.b3 = 0.0f;
    matrix.b4 = 0.0f;
    matrix.c1 = 0.0f;
    matrix.c2 = 0.0f;
    matrix.c3 = 1.0f;
    matrix.c4 = 0.0f;
    matrix.d1 = 0.0f;
    matrix.d2 = 0.0f;
    matrix.d3 = 0.0f;
    matrix.d4 = 1.0f;
    return matrix;
}

static int MikuPan_GltfNodeIndexFromString(const char *name)
{
    if (name == nullptr || strncmp(name, "Node", 4) != 0)
    {
        return -1;
    }

    const char *digits = name + 4;
    if (*digits < '0' || *digits > '9')
    {
        return -1;
    }

    char *end = nullptr;
    long value = strtol(digits, &end, 10);
    if (end == digits || value < 0 || value > INT_MAX)
    {
        return -1;
    }

    return (int)value;
}

static int MikuPan_GltfNodeIndexFromName(const aiNode *node)
{
    if (node == nullptr)
    {
        return -1;
    }

    return MikuPan_GltfNodeIndexFromString(node->mName.C_Str());
}

static void MikuPan_GltfCollectMeshNodes(
    const aiNode *node,
    const aiMatrix4x4 &parent_matrix,
    int inherited_node_index,
    const aiMatrix4x4 &inherited_node_matrix,
    std::vector<int> &mesh_node_indices,
    std::vector<std::array<float, 16>> &mesh_bind_matrices,
    std::vector<std::array<float, 16>> &mesh_node_offset_matrices)
{
    if (node == nullptr)
    {
        return;
    }

    aiMatrix4x4 global_matrix = parent_matrix * node->mTransformation;
    int node_index = MikuPan_GltfNodeIndexFromName(node);
    aiMatrix4x4 node_base_matrix = inherited_node_matrix;
    if (node_index >= 0)
    {
        node_base_matrix = global_matrix;
    }
    else
    {
        node_index = inherited_node_index;
    }

    if (node_index < 0)
    {
        node_base_matrix = global_matrix;
    }

    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        unsigned int mesh_index = node->mMeshes[i];
        if (mesh_index >= mesh_node_indices.size())
        {
            continue;
        }

        mesh_node_indices[mesh_index] = node_index;
        MikuPan_GltfCopyAiMatrix(mesh_bind_matrices[mesh_index].data(),
                                 global_matrix);
        if (node_index >= 0)
        {
            MikuPan_GltfCopyAiRelativeMatrix(
                mesh_node_offset_matrices[mesh_index].data(),
                node_base_matrix,
                global_matrix);
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        MikuPan_GltfCollectMeshNodes(node->mChildren[i],
                                     global_matrix,
                                     node_index,
                                     node_base_matrix,
                                     mesh_node_indices,
                                     mesh_bind_matrices,
                                     mesh_node_offset_matrices);
    }
}

static void MikuPan_GltfCollectNodes(
    const aiNode *node,
    const aiMatrix4x4 &parent_matrix,
    std::unordered_map<std::string, int> &node_indices_by_name,
    std::unordered_map<int, std::array<float, 16>> &node_bind_matrices)
{
    if (node == nullptr)
    {
        return;
    }

    aiMatrix4x4 global_matrix = parent_matrix * node->mTransformation;
    const char *name = node->mName.C_Str();
    int node_index = MikuPan_GltfNodeIndexFromString(name);
    if (node_index >= 0)
    {
        node_indices_by_name[name != nullptr ? name : ""] = node_index;

        std::array<float, 16> bind_matrix = {};
        MikuPan_GltfCopyAiMatrix(bind_matrix.data(), global_matrix);
        node_bind_matrices[node_index] = bind_matrix;
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        MikuPan_GltfCollectNodes(node->mChildren[i],
                                 global_matrix,
                                 node_indices_by_name,
                                 node_bind_matrices);
    }
}

static std::string MikuPan_GltfDecodeUriPath(std::string path)
{
    for (size_t pos = 0; (pos = path.find("%20", pos)) != std::string::npos;)
    {
        path.replace(pos, 3, " ");
        pos++;
    }

    for (char &ch : path)
    {
        if (ch == '/')
        {
            ch = std::filesystem::path::preferred_separator;
        }
    }

    return path;
}

static unsigned int MikuPan_GltfCreateWhiteTexture(void)
{
    const unsigned char white[4] = {255, 255, 255, 255};
    return MikuPan_GPUCreateTextureRGBA8(1, 1, white, 4, 1, 0);
}

static bool MikuPan_GltfDecodeImageFile(const std::filesystem::path &path,
                                        std::vector<unsigned char> &rgba,
                                        int &width,
                                        int &height)
{
    AVFormatContext *fmt = nullptr;
    AVCodecContext *decoder = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    AVFrame *rgba_frame = nullptr;
    SwsContext *sws = nullptr;
    bool ok = false;

    auto cleanup = [&]() {
        if (sws != nullptr)
        {
            sws_freeContext(sws);
        }
        if (rgba_frame != nullptr)
        {
            av_frame_free(&rgba_frame);
        }
        if (frame != nullptr)
        {
            av_frame_free(&frame);
        }
        if (packet != nullptr)
        {
            av_packet_free(&packet);
        }
        if (decoder != nullptr)
        {
            avcodec_free_context(&decoder);
        }
        if (fmt != nullptr)
        {
            avformat_close_input(&fmt);
        }
    };

    std::string filename = path.string();
    if (avformat_open_input(&fmt, filename.c_str(), nullptr, nullptr) < 0)
    {
        cleanup();
        return false;
    }

    avformat_find_stream_info(fmt, nullptr);
    int stream_index =
        av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (stream_index < 0)
    {
        cleanup();
        return false;
    }

    AVStream *stream = fmt->streams[stream_index];
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (codec == nullptr)
    {
        cleanup();
        return false;
    }

    decoder = avcodec_alloc_context3(codec);
    if (decoder == nullptr ||
        avcodec_parameters_to_context(decoder, stream->codecpar) < 0 ||
        avcodec_open2(decoder, codec, nullptr) < 0)
    {
        cleanup();
        return false;
    }

    packet = av_packet_alloc();
    frame = av_frame_alloc();
    rgba_frame = av_frame_alloc();
    if (packet == nullptr || frame == nullptr || rgba_frame == nullptr)
    {
        cleanup();
        return false;
    }

    while (!ok && av_read_frame(fmt, packet) >= 0)
    {
        if (packet->stream_index != stream_index)
        {
            av_packet_unref(packet);
            continue;
        }

        if (avcodec_send_packet(decoder, packet) >= 0)
        {
            while (avcodec_receive_frame(decoder, frame) == 0)
            {
                width = frame->width;
                height = frame->height;
                if (width <= 0 || height <= 0)
                {
                    av_frame_unref(frame);
                    continue;
                }

                sws = sws_getContext(width,
                                     height,
                                     (AVPixelFormat)frame->format,
                                     width,
                                     height,
                                     AV_PIX_FMT_RGBA,
                                     SWS_BILINEAR,
                                     nullptr,
                                     nullptr,
                                     nullptr);
                if (sws == nullptr)
                {
                    av_frame_unref(frame);
                    continue;
                }

                int buffer_size =
                    av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
                if (buffer_size <= 0)
                {
                    av_frame_unref(frame);
                    continue;
                }
                rgba.resize((size_t)buffer_size);
                av_image_fill_arrays(rgba_frame->data,
                                     rgba_frame->linesize,
                                     rgba.data(),
                                     AV_PIX_FMT_RGBA,
                                     width,
                                     height,
                                     1);
                sws_scale(sws,
                          frame->data,
                          frame->linesize,
                          0,
                          height,
                          rgba_frame->data,
                          rgba_frame->linesize);

                ok = true;
                av_frame_unref(frame);
                break;
            }
        }

        av_packet_unref(packet);
    }

    cleanup();
    return ok;
}

static void MikuPan_GltfFixFullyTransparentTexture(std::vector<unsigned char> &rgba)
{
    if (rgba.empty())
    {
        return;
    }

    int has_visible_alpha = 0;
    for (size_t i = 3; i < rgba.size(); i += 4)
    {
        if (rgba[i] > 1)
        {
            has_visible_alpha = 1;
            break;
        }
    }

    if (has_visible_alpha)
    {
        return;
    }

    for (size_t i = 3; i < rgba.size(); i += 4)
    {
        rgba[i] = 255;
    }
}

static unsigned int MikuPan_GltfLoadTextureFile(const std::filesystem::path &path,
                                                int &width,
                                                int &height)
{
    std::vector<unsigned char> rgba;
    if (!MikuPan_GltfDecodeImageFile(path, rgba, width, height))
    {
        return 0;
    }

    MikuPan_GltfFixFullyTransparentTexture(rgba);
    return MikuPan_GPUCreateTextureRGBA8(
        width, height, rgba.data(), width * 4, 1, 1);
}

static void MikuPan_GltfInitMaterial(MikuPanGltfMaterial &material)
{
    material.texture = {};
    material.texture_valid = 0;
    material.base_color[0] = 1.0f;
    material.base_color[1] = 1.0f;
    material.base_color[2] = 1.0f;
    material.base_color[3] = 1.0f;
}

static void MikuPan_GltfLoadMaterialTexture(const aiScene *scene,
                                            const aiMaterial *ai_material,
                                            const std::filesystem::path &model_dir,
                                            MikuPanGltfMaterial &material)
{
    aiString texture_name;
    aiTextureType texture_type = aiTextureType_BASE_COLOR;
    if (ai_material->GetTextureCount(texture_type) == 0)
    {
        texture_type = aiTextureType_DIFFUSE;
    }

    if (ai_material->GetTextureCount(texture_type) == 0 ||
        ai_material->GetTexture(texture_type, 0, &texture_name) != AI_SUCCESS)
    {
        material.texture.id = MikuPan_GltfCreateWhiteTexture();
        material.texture.width = 1;
        material.texture.height = 1;
        material.texture_valid = material.texture.id != 0;
        return;
    }

    std::string texture_path = texture_name.C_Str();
    material.texture_path = texture_path;

    if (!texture_path.empty() && texture_path[0] == '*')
    {
        int embedded_index = atoi(texture_path.c_str() + 1);
        if (scene != nullptr && embedded_index >= 0 &&
            embedded_index < (int)scene->mNumTextures)
        {
            const aiTexture *texture = scene->mTextures[embedded_index];
            if (texture != nullptr && texture->mHeight != 0 &&
                texture->pcData != nullptr)
            {
                int width = (int)texture->mWidth;
                int height = (int)texture->mHeight;
                std::vector<unsigned char> rgba((size_t)width *
                                                (size_t)height * 4u);
                for (int i = 0; i < width * height; i++)
                {
                    rgba[(size_t)i * 4u + 0u] = texture->pcData[i].r;
                    rgba[(size_t)i * 4u + 1u] = texture->pcData[i].g;
                    rgba[(size_t)i * 4u + 2u] = texture->pcData[i].b;
                    rgba[(size_t)i * 4u + 3u] = texture->pcData[i].a;
                }
                MikuPan_GltfFixFullyTransparentTexture(rgba);
                material.texture.id = MikuPan_GPUCreateTextureRGBA8(
                    width, height, rgba.data(), width * 4, 1, 1);
                material.texture.width = width;
                material.texture.height = height;
                material.texture_valid = material.texture.id != 0;
                return;
            }
        }

        warn_log("[GLTF] compressed embedded texture is not decoded yet: %s",
                 texture_path.c_str());
    }
    else
    {
        std::filesystem::path resolved =
            model_dir / MikuPan_GltfDecodeUriPath(texture_path);
        int width = 0;
        int height = 0;
        material.texture.id = MikuPan_GltfLoadTextureFile(resolved, width, height);
        if (material.texture.id != 0)
        {
            material.texture.width = width;
            material.texture.height = height;
            material.texture_valid = 1;
            material.texture_path = resolved.string();
            return;
        }

        warn_log("[GLTF] failed to decode texture: %s", resolved.string().c_str());
    }

    material.texture.id = MikuPan_GltfCreateWhiteTexture();
    material.texture.width = 1;
    material.texture.height = 1;
    material.texture_valid = material.texture.id != 0;
}

static void MikuPan_GltfLoadMaterials(MikuPan_GltfModel *model,
                                      const aiScene *scene)
{
    unsigned int material_count = scene->mNumMaterials;
    if (material_count == 0)
    {
        material_count = 1;
    }

    model->materials.resize(material_count);
    std::filesystem::path model_dir = model->path.parent_path();

    for (unsigned int i = 0; i < material_count; i++)
    {
        MikuPanGltfMaterial &material = model->materials[i];
        MikuPan_GltfInitMaterial(material);

        const aiMaterial *ai_material =
            i < scene->mNumMaterials ? scene->mMaterials[i] : nullptr;
        if (ai_material != nullptr)
        {
            aiColor4D color;
            if (aiGetMaterialColor(ai_material, AI_MATKEY_BASE_COLOR, &color) ==
                    AI_SUCCESS ||
                aiGetMaterialColor(ai_material, AI_MATKEY_COLOR_DIFFUSE, &color) ==
                    AI_SUCCESS)
            {
                material.base_color[0] = color.r;
                material.base_color[1] = color.g;
                material.base_color[2] = color.b;
                material.base_color[3] = color.a;
            }

            MikuPan_GltfLoadMaterialTexture(scene, ai_material, model_dir, material);
        }
        else
        {
            material.texture.id = MikuPan_GltfCreateWhiteTexture();
            material.texture.width = 1;
            material.texture.height = 1;
            material.texture_valid = material.texture.id != 0;
        }

        if (material.texture_valid)
        {
            model->texture_count++;
        }
    }
}

static void MikuPan_GltfAddSkinWeight(MikuPanGltfSkinVertex &skin,
                                      int joint_index,
                                      float weight)
{
    if (joint_index < 0 || weight <= 0.0f)
    {
        return;
    }

    int slot = -1;
    for (int i = 0; i < 4; i++)
    {
        if (skin.weights[i] <= 0.0f)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        slot = 0;
        for (int i = 1; i < 4; i++)
        {
            if (skin.weights[i] < skin.weights[slot])
            {
                slot = i;
            }
        }

        if (weight <= skin.weights[slot])
        {
            return;
        }
    }

    skin.joints[slot] = (float)joint_index;
    skin.weights[slot] = weight;
}

static void MikuPan_GltfNormalizeSkinWeights(MikuPanGltfSkinVertex &skin)
{
    float sum = 0.0f;
    for (int i = 0; i < 4; i++)
    {
        sum += skin.weights[i];
    }

    if (sum <= 0.0f)
    {
        skin.joints[0] = 0.0f;
        skin.weights[0] = 1.0f;
        return;
    }

    for (int i = 0; i < 4; i++)
    {
        skin.weights[i] /= sum;
    }
}

static int MikuPan_GltfFindBoneNodeIndex(
    const aiBone *bone,
    const std::unordered_map<std::string, int> &node_indices_by_name)
{
    if (bone == nullptr)
    {
        return -1;
    }

    const char *name = bone->mName.C_Str();
    int node_index = MikuPan_GltfNodeIndexFromString(name);
    if (node_index >= 0)
    {
        return node_index;
    }

    auto it = node_indices_by_name.find(name != nullptr ? name : "");
    if (it != node_indices_by_name.end())
    {
        return it->second;
    }

    return -1;
}

static bool MikuPan_GltfBuildSkinData(
    const aiMesh *mesh,
    const std::unordered_map<std::string, int> &node_indices_by_name,
    const std::unordered_map<int, std::array<float, 16>> &node_bind_matrices,
    std::vector<MikuPanGltfSkinVertex> &skin_vertices,
    std::vector<MikuPanGltfBone> &bones)
{
    if (mesh == nullptr || !mesh->HasBones() || mesh->mNumVertices == 0)
    {
        return false;
    }

    skin_vertices.assign(mesh->mNumVertices, MikuPanGltfSkinVertex{});
    bones.clear();

    for (unsigned int bone_index = 0; bone_index < mesh->mNumBones; bone_index++)
    {
        const aiBone *ai_bone = mesh->mBones[bone_index];
        int node_index =
            MikuPan_GltfFindBoneNodeIndex(ai_bone, node_indices_by_name);
        if (node_index < 0)
        {
            continue;
        }

        MikuPanGltfBone bone = {};
        bone.node_index = node_index;
        MikuPan_GltfCopyAiMatrix(bone.inverse_bind_matrix,
                                 ai_bone->mOffsetMatrix);

        auto bind_it = node_bind_matrices.find(node_index);
        if (bind_it != node_bind_matrices.end())
        {
            memcpy(bone.bind_global_matrix,
                   bind_it->second.data(),
                   sizeof(bone.bind_global_matrix));
        }
        else
        {
            MikuPan_GltfSetIdentityMatrix(bone.bind_global_matrix);
        }

        int joint_index = (int)bones.size();
        bones.push_back(bone);

        for (unsigned int weight_index = 0;
             weight_index < ai_bone->mNumWeights;
             weight_index++)
        {
            const aiVertexWeight &weight = ai_bone->mWeights[weight_index];
            if (weight.mVertexId >= mesh->mNumVertices)
            {
                continue;
            }

            MikuPan_GltfAddSkinWeight(
                skin_vertices[weight.mVertexId],
                joint_index,
                weight.mWeight);
        }
    }

    if (bones.empty())
    {
        skin_vertices.clear();
        return false;
    }

    for (MikuPanGltfSkinVertex &skin_vertex : skin_vertices)
    {
        MikuPan_GltfNormalizeSkinWeights(skin_vertex);
    }

    return true;
}

static bool MikuPan_GltfBuildMesh(MikuPan_GltfModel *model,
                                  const aiMesh *mesh,
                                  int node_index,
                                  const float *bind_matrix,
                                  const float *node_offset_matrix,
                                  const std::unordered_map<std::string, int> &node_indices_by_name,
                                  const std::unordered_map<int, std::array<float, 16>> &node_bind_matrices)
{
    if (mesh == nullptr || mesh->mNumVertices == 0 || mesh->mNumFaces == 0)
    {
        return false;
    }

    std::vector<MikuPanGltfPositionNormal> positions(mesh->mNumVertices);
    std::vector<float> uvs((size_t)mesh->mNumVertices * 2u, 0.0f);
    std::vector<float> colors((size_t)mesh->mNumVertices * 3u, 1.0f);
    std::vector<MikuPanGltfSkinVertex> skin_vertices;
    std::vector<MikuPanGltfBone> bones;
    bool is_skinned = MikuPan_GltfBuildSkinData(mesh,
                                                node_indices_by_name,
                                                node_bind_matrices,
                                                skin_vertices,
                                                bones);

    int material_index = (int)mesh->mMaterialIndex;
    if (material_index < 0 || material_index >= (int)model->materials.size())
    {
        material_index = 0;
    }

    const MikuPanGltfMaterial *material =
        !model->materials.empty() ? &model->materials[material_index] : nullptr;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        positions[i].position[0] = mesh->mVertices[i].x;
        positions[i].position[1] = mesh->mVertices[i].y;
        positions[i].position[2] = mesh->mVertices[i].z;

        if (mesh->HasNormals())
        {
            positions[i].normal[0] = mesh->mNormals[i].x;
            positions[i].normal[1] = mesh->mNormals[i].y;
            positions[i].normal[2] = mesh->mNormals[i].z;
        }
        else
        {
            positions[i].normal[0] = 0.0f;
            positions[i].normal[1] = 1.0f;
            positions[i].normal[2] = 0.0f;
        }

        if (mesh->HasTextureCoords(0))
        {
            uvs[(size_t)i * 2u + 0u] = mesh->mTextureCoords[0][i].x;
            uvs[(size_t)i * 2u + 1u] = mesh->mTextureCoords[0][i].y;
        }

        const float neutral_color = 128.0f / 255.0f;
        float color[3] = {neutral_color, neutral_color, neutral_color};
        if (mesh->HasVertexColors(0))
        {
            color[0] = mesh->mColors[0][i].r;
            color[1] = mesh->mColors[0][i].g;
            color[2] = mesh->mColors[0][i].b;
        }

        if (material != nullptr)
        {
            color[0] *= material->base_color[0];
            color[1] *= material->base_color[1];
            color[2] *= material->base_color[2];
        }

        colors[(size_t)i * 3u + 0u] = color[0];
        colors[(size_t)i * 3u + 1u] = color[1];
        colors[(size_t)i * 3u + 2u] = color[2];
    }

    std::vector<unsigned int> indices;
    indices.reserve((size_t)mesh->mNumFaces * 3u);
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        const aiFace &face = mesh->mFaces[i];
        if (face.mNumIndices != 3)
        {
            continue;
        }

        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);
    }

    if (indices.empty())
    {
        return false;
    }

    MikuPanGltfMesh render_mesh = {};
    render_mesh.vertex_count = (int)mesh->mNumVertices;
    render_mesh.index_count = (int)indices.size();
    render_mesh.material_index = material_index;
    render_mesh.node_index = node_index;
    render_mesh.skinned = is_skinned ? 1 : 0;
    render_mesh.bones = bones;
    render_mesh.skin_palette_offset = model->skin_palette_joint_count;
    if (render_mesh.skinned)
    {
        for (MikuPanGltfSkinVertex &skin_vertex : skin_vertices)
        {
            for (int i = 0; i < 4; i++)
            {
                skin_vertex.joints[i] += (float)render_mesh.skin_palette_offset;
            }
        }
    }
    if (bind_matrix != nullptr)
    {
        memcpy(render_mesh.bind_matrix,
               bind_matrix,
               sizeof(render_mesh.bind_matrix));
    }
    else
    {
        MikuPan_GltfSetIdentityMatrix(render_mesh.bind_matrix);
    }
    if (node_offset_matrix != nullptr)
    {
        memcpy(render_mesh.node_offset_matrix,
               node_offset_matrix,
               sizeof(render_mesh.node_offset_matrix));
    }
    else
    {
        MikuPan_GltfSetIdentityMatrix(render_mesh.node_offset_matrix);
    }

    render_mesh.vertex_buffer = MikuPan_GPUCreateBuffer(
        (unsigned int)(positions.size() * sizeof(MikuPanGltfPositionNormal)),
        MIKUPAN_GPU_BUFFER_VERTEX);
    render_mesh.uv_buffer = MikuPan_GPUCreateBuffer(
        (unsigned int)(uvs.size() * sizeof(float)), MIKUPAN_GPU_BUFFER_VERTEX);
    render_mesh.color_buffer = MikuPan_GPUCreateBuffer(
        (unsigned int)(colors.size() * sizeof(float)), MIKUPAN_GPU_BUFFER_VERTEX);
    if (render_mesh.skinned)
    {
        render_mesh.skin_buffer = MikuPan_GPUCreateBuffer(
            (unsigned int)(skin_vertices.size() * sizeof(MikuPanGltfSkinVertex)),
            MIKUPAN_GPU_BUFFER_VERTEX);
    }
    render_mesh.index_buffer = MikuPan_GPUCreateBuffer(
        (unsigned int)(indices.size() * sizeof(unsigned int)),
        MIKUPAN_GPU_BUFFER_INDEX);

    if (render_mesh.vertex_buffer == 0 || render_mesh.uv_buffer == 0 ||
        render_mesh.color_buffer == 0 || render_mesh.index_buffer == 0 ||
        (render_mesh.skinned && render_mesh.skin_buffer == 0))
    {
        MikuPan_GPUReleaseBuffer(render_mesh.vertex_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.uv_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.color_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.skin_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.index_buffer);
        return false;
    }

    MikuPan_GPUUploadBuffer(render_mesh.vertex_buffer,
                            (unsigned int)(positions.size() *
                                           sizeof(MikuPanGltfPositionNormal)),
                            positions.data());
    MikuPan_GPUUploadBuffer(render_mesh.uv_buffer,
                            (unsigned int)(uvs.size() * sizeof(float)),
                            uvs.data());
    MikuPan_GPUUploadBuffer(render_mesh.color_buffer,
                            (unsigned int)(colors.size() * sizeof(float)),
                            colors.data());
    if (render_mesh.skinned)
    {
        MikuPan_GPUUploadBuffer(
            render_mesh.skin_buffer,
            (unsigned int)(skin_vertices.size() *
                           sizeof(MikuPanGltfSkinVertex)),
            skin_vertices.data());
    }
    MikuPan_GPUUploadBuffer(render_mesh.index_buffer,
                            (unsigned int)(indices.size() *
                                           sizeof(unsigned int)),
                            indices.data());

    if (render_mesh.skinned)
    {
        unsigned int buffers[4] = {
            render_mesh.vertex_buffer,
            render_mesh.uv_buffer,
            render_mesh.color_buffer,
            render_mesh.skin_buffer,
        };
        render_mesh.vao = MikuPan_GPURegisterVertexArray(
            GLTF_SKIN_POSITION3_NORMAL3_UV2_JOINTS4_WEIGHTS4,
            4,
            buffers,
            render_mesh.index_buffer);
    }
    else
    {
        unsigned int buffers[3] = {
            render_mesh.vertex_buffer,
            render_mesh.uv_buffer,
            render_mesh.color_buffer,
        };
        render_mesh.vao = MikuPan_GPURegisterVertexArray(
            POSITION3_NORMAL3_UV2, 3, buffers, render_mesh.index_buffer);
    }
    if (render_mesh.vao == 0)
    {
        MikuPan_GPUReleaseBuffer(render_mesh.vertex_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.uv_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.color_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.skin_buffer);
        MikuPan_GPUReleaseBuffer(render_mesh.index_buffer);
        return false;
    }

    model->vertex_count += render_mesh.vertex_count;
    model->index_count += render_mesh.index_count;
    if (render_mesh.skinned)
    {
        model->skinned_mesh_count++;
        model->bone_count += (int)render_mesh.bones.size();
        model->skin_palette_joint_count += (int)render_mesh.bones.size();
    }
    model->meshes.push_back(render_mesh);
    return true;
}

MikuPan_GltfModel *MikuPan_LoadGltfModel(const char *path)
{
    if (path == nullptr || path[0] == '\0')
    {
        return nullptr;
    }

    Assimp::Importer importer;
    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_ValidateDataStructure;

    const aiScene *scene = importer.ReadFile(path, flags);
    if (scene == nullptr || scene->mNumMeshes == 0)
    {
        error_log("[GLTF] import failed for %s: %s",
                  path,
                  importer.GetErrorString());
        return nullptr;
    }

    auto *model = new MikuPan_GltfModel();
    model->path = path;

    MikuPan_GltfLoadMaterials(model, scene);

    std::vector<int> mesh_node_indices(scene->mNumMeshes, -1);
    std::vector<std::array<float, 16>> mesh_bind_matrices(scene->mNumMeshes);
    std::vector<std::array<float, 16>> mesh_node_offset_matrices(
        scene->mNumMeshes);
    for (std::array<float, 16> &matrix : mesh_bind_matrices)
    {
        MikuPan_GltfSetIdentityMatrix(matrix.data());
    }
    for (std::array<float, 16> &matrix : mesh_node_offset_matrices)
    {
        MikuPan_GltfSetIdentityMatrix(matrix.data());
    }

    aiMatrix4x4 identity_matrix = MikuPan_GltfMakeAiIdentityMatrix();
    std::unordered_map<std::string, int> node_indices_by_name;
    std::unordered_map<int, std::array<float, 16>> node_bind_matrices;
    MikuPan_GltfCollectNodes(scene->mRootNode,
                             identity_matrix,
                             node_indices_by_name,
                             node_bind_matrices);
    MikuPan_GltfCollectMeshNodes(scene->mRootNode,
                                 identity_matrix,
                                 -1,
                                 identity_matrix,
                                 mesh_node_indices,
                                 mesh_bind_matrices,
                                 mesh_node_offset_matrices);

    for (int node_index : mesh_node_indices)
    {
        if (node_index >= 0)
        {
            model->node_mapped_mesh_count++;
        }
        else
        {
            model->node_unmapped_mesh_count++;
        }
    }

    for (unsigned int i = 0; i < scene->mNumMeshes; i++)
    {
        if (!MikuPan_GltfBuildMesh(model,
                                   scene->mMeshes[i],
                                   mesh_node_indices[i],
                                   mesh_bind_matrices[i].data(),
                                   mesh_node_offset_matrices[i].data(),
                                   node_indices_by_name,
                                   node_bind_matrices))
        {
            warn_log("[GLTF] skipped mesh %u in %s", i, path);
        }
    }

    if (model->meshes.empty())
    {
        MikuPan_FreeGltfModel(model);
        return nullptr;
    }

    if (model->skin_palette_joint_count > 0)
    {
        model->skin_palette_buffer = MikuPan_GPUCreateBuffer(
            (unsigned int)((size_t)model->skin_palette_joint_count * 64u),
            MIKUPAN_GPU_BUFFER_STORAGE);
        if (model->skin_palette_buffer == 0)
        {
            warn_log("[GLTF] failed to allocate shared skin palette buffer for %s",
                     path);
        }
    }

    info_log("[GLTF] loaded %s: %d meshes, %d skinned, %d bones, %d vertices, %d indices, %d node-mapped, %d static",
             path,
             (int)model->meshes.size(),
             model->skinned_mesh_count,
             model->bone_count,
             model->vertex_count,
             model->index_count,
             model->node_mapped_mesh_count,
             model->node_unmapped_mesh_count);
    return model;
}

void MikuPan_FreeGltfModel(MikuPan_GltfModel *model)
{
    if (model == nullptr)
    {
        return;
    }

    for (MikuPanGltfMesh &mesh : model->meshes)
    {
        MikuPan_GPUReleaseVertexArray(mesh.vao);
        MikuPan_GPUReleaseBuffer(mesh.vertex_buffer);
        MikuPan_GPUReleaseBuffer(mesh.uv_buffer);
        MikuPan_GPUReleaseBuffer(mesh.color_buffer);
        MikuPan_GPUReleaseBuffer(mesh.skin_buffer);
        MikuPan_GPUReleaseBuffer(mesh.index_buffer);
    }

    MikuPan_GPUReleaseBuffer(model->skin_palette_buffer);

    for (MikuPanGltfMaterial &material : model->materials)
    {
        if (material.texture_valid && material.texture.id != 0)
        {
            MikuPan_GPUReleaseTexture(material.texture.id);
        }
    }

    delete model;
}

static void MikuPan_GltfSetRenderState(void)
{
    if (MikuPan_IsShadowReceiverPassActive())
    {
        MikuPan_SetRenderStateShadowReceiver();
    }
    else if (MikuPan_IsShadowPassActive())
    {
        MikuPan_SetRenderStateShadow();
    }
    else if (MikuPan_IsMirrorReflectionPass())
    {
        MikuPan_SetRenderState3DDoubleSided();
    }
    else
    {
        MikuPan_SetRenderState3DDoubleSided();
    }
}

static int MikuPan_GltfBuildMeshModelMatrix(const MikuPanGltfMesh &mesh,
                                            const float *matrices,
                                            int matrix_count,
                                            int require_mapped_matrix,
                                            sceVu0FMATRIX out_matrix)
{
    if (matrices != nullptr && mesh.node_index >= 0 &&
        mesh.node_index < matrix_count)
    {
        sceVu0FMATRIX node_matrix;
        sceVu0FMATRIX node_offset_matrix;
        memcpy(node_matrix,
               matrices + ((size_t)mesh.node_index * 16u),
               sizeof(node_matrix));
        memcpy(node_offset_matrix,
               mesh.node_offset_matrix,
               sizeof(node_offset_matrix));
        sceVu0MulMatrix(out_matrix, node_matrix, node_offset_matrix);
        return 1;
    }

    if (matrices != nullptr && require_mapped_matrix)
    {
        return 0;
    }

    memcpy(out_matrix, mesh.bind_matrix, sizeof(sceVu0FMATRIX));
    return 1;
}

static void MikuPan_GltfBuildSkinPalette(const MikuPanGltfMesh &mesh,
                                         const float *matrices,
                                         int matrix_count,
                                         sceVu0FMATRIX mesh_matrix,
                                         std::vector<std::array<float, 16>> &palette)
{
    size_t palette_end =
        (size_t)mesh.skin_palette_offset + mesh.bones.size();
    if (palette.size() < palette_end)
    {
        palette.resize(palette_end);
    }

    sceVu0FMATRIX inverse_mesh_matrix;
    sceVu0InversMatrix(inverse_mesh_matrix, mesh_matrix);

    for (size_t i = 0; i < mesh.bones.size(); i++)
    {
        const MikuPanGltfBone &bone = mesh.bones[i];
        const float *joint_source = bone.bind_global_matrix;
        if (matrices != nullptr && bone.node_index >= 0 &&
            bone.node_index < matrix_count)
        {
            joint_source = matrices + ((size_t)bone.node_index * 16u);
        }

        sceVu0FMATRIX joint_matrix;
        sceVu0FMATRIX inverse_bind_matrix;
        sceVu0FMATRIX joint_model_matrix;
        sceVu0FMATRIX skin_matrix;
        memcpy(joint_matrix, joint_source, sizeof(joint_matrix));
        memcpy(inverse_bind_matrix,
               bone.inverse_bind_matrix,
               sizeof(inverse_bind_matrix));

        sceVu0MulMatrix(joint_model_matrix, joint_matrix, inverse_bind_matrix);
        sceVu0MulMatrix(skin_matrix, inverse_mesh_matrix, joint_model_matrix);
        memcpy(palette[(size_t)mesh.skin_palette_offset + i].data(),
               skin_matrix,
               sizeof(skin_matrix));
    }
}

static unsigned long long MikuPan_GltfFnv1aUpdate(unsigned long long hash,
                                                  const void *data,
                                                  size_t size)
{
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < size; i++)
    {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }

    return hash;
}

static unsigned long long MikuPan_GltfHashSkinPose(const MikuPan_GltfModel *model,
                                                   const float *matrices,
                                                   int matrix_count)
{
    unsigned long long hash = 1469598103934665603ull;
    hash = MikuPan_GltfFnv1aUpdate(hash,
                                   &matrix_count,
                                   sizeof(matrix_count));
    hash = MikuPan_GltfFnv1aUpdate(hash,
                                   &model->skin_palette_joint_count,
                                   sizeof(model->skin_palette_joint_count));

    if (matrices != nullptr && matrix_count > 0)
    {
        hash = MikuPan_GltfFnv1aUpdate(hash,
                                       matrices,
                                       (size_t)matrix_count * 16u *
                                           sizeof(float));
    }
    else
    {
        static const unsigned int bind_pose_marker = 0x474c5446u;
        hash = MikuPan_GltfFnv1aUpdate(hash,
                                       &bind_pose_marker,
                                       sizeof(bind_pose_marker));
    }

    return hash;
}

static void MikuPan_GltfPrepareSkinPalettes(const MikuPan_GltfModel *model,
                                            const float *matrices,
                                            int matrix_count)
{
    if (model == nullptr || model->skin_palette_buffer == 0 ||
        model->skin_palette_joint_count <= 0)
    {
        return;
    }

    const unsigned long long pose_hash =
        MikuPan_GltfHashSkinPose(model, matrices, matrix_count);
    if (model->skin_palette_uploaded_valid &&
        model->skin_palette_uploaded_hash == pose_hash)
    {
        return;
    }

    std::vector<std::array<float, 16>> skin_palette;
    skin_palette.resize((size_t)model->skin_palette_joint_count);

    for (const MikuPanGltfMesh &mesh : model->meshes)
    {
        if (!mesh.skinned || mesh.vao == 0 || mesh.index_count <= 0)
        {
            continue;
        }

        sceVu0FMATRIX model_matrix;
        if (!MikuPan_GltfBuildMeshModelMatrix(mesh,
                                              matrices,
                                              matrix_count,
                                              0,
                                              model_matrix))
        {
            continue;
        }

        MikuPan_GltfBuildSkinPalette(mesh,
                                     matrices,
                                     matrix_count,
                                     model_matrix,
                                     skin_palette);
    }

    MikuPan_GPUUploadBuffer(
        model->skin_palette_buffer,
        (unsigned int)(skin_palette.size() *
                       sizeof(std::array<float, 16>)),
        skin_palette.data());

    model->skin_palette_uploaded_hash = pose_hash;
    model->skin_palette_uploaded_valid = 1;
}

void MikuPan_RenderGltfModelWithNodeMatrices(const MikuPan_GltfModel *model,
                                             const float *matrices,
                                             int matrix_count)
{
    if (model == nullptr)
    {
        return;
    }

    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    MikuPan_FlushTexturedSpriteBatch();
    char texture_uniform[] = "uTexture";
    const float previous_material_alpha = MikuPan_GetCurrentMaterialAlpha();
    if (previous_material_alpha != 1.0f)
    {
        MikuPan_SetCurrentMaterialAlpha(1.0f);
    }

    static int warned_missing_gltf_skin_shader = 0;
    const int gltf_skin_shader_available =
        MikuPan_IsShaderAvailable(GLTF_SKINNED_SHADER);
    if (model->skinned_mesh_count > 0 && gltf_skin_shader_available)
    {
        MikuPan_GltfPrepareSkinPalettes(model, matrices, matrix_count);
    }

    MikuPan_GltfSetRenderState();

    for (const MikuPanGltfMesh &mesh : model->meshes)
    {
        if (mesh.vao == 0 || mesh.index_count <= 0)
        {
            continue;
        }

        const MikuPanGltfMaterial *material = nullptr;
        if (mesh.material_index >= 0 &&
            mesh.material_index < (int)model->materials.size())
        {
            material = &model->materials[mesh.material_index];
        }

        unsigned int texture_id =
            material != nullptr && material->texture_valid
                ? material->texture.id
                : 0;

        sceVu0FMATRIX model_matrix;
        if (mesh.skinned)
        {
            if (!gltf_skin_shader_available || model->skin_palette_buffer == 0)
            {
                if (!warned_missing_gltf_skin_shader)
                {
                    warn_log("[GLTF] skinned meshes present but GLTF_SKINNED shader bytecode or palette buffer is missing");
                    warned_missing_gltf_skin_shader = 1;
                }
                continue;
            }

            if (!MikuPan_GltfBuildMeshModelMatrix(mesh,
                                                  matrices,
                                                  matrix_count,
                                                  0,
                                                  model_matrix))
            {
                continue;
            }

            if (MikuPan_SetCurrentShaderProgram(GLTF_SKINNED_SHADER) ==
                (u_int)-1)
            {
                continue;
            }
            MikuPan_SetUniform1iToCurrentShader(0, texture_uniform);
            MikuPan_SetModelTransformMatrix(model_matrix);
            MikuPan_GPUSetVertexStorageBuffer(model->skin_palette_buffer);
        }
        else
        {
            if (!MikuPan_GltfBuildMeshModelMatrix(mesh,
                                                  matrices,
                                                  matrix_count,
                                                  0,
                                                  model_matrix))
            {
                continue;
            }

            if (MikuPan_SetCurrentShaderProgram(MESH_0x12_SHADER) ==
                (u_int)-1)
            {
                continue;
            }
            MikuPan_SetUniform1iToCurrentShader(0, texture_uniform);
            MikuPan_SetModelTransformMatrix(model_matrix);
        }

        MikuPan_BindTexture2DCached(texture_id);
        MikuPan_BindVAO(mesh.vao);
        MikuPan_PerfDrawCall();
        MikuPan_TimedDrawElements(GL_TRIANGLES,
                                  mesh.index_count,
                                  GL_UNSIGNED_INT,
                                  (void *)0);

        if (mesh.skinned)
        {
            MikuPan_GPUSetVertexStorageBuffer(0);
        }
    }

    if (previous_material_alpha != 1.0f)
    {
        MikuPan_SetCurrentMaterialAlpha(previous_material_alpha);
    }
}

void MikuPan_RenderGltfModel(const MikuPan_GltfModel *model)
{
    MikuPan_RenderGltfModelWithNodeMatrices(model, nullptr, 0);
}

int MikuPan_GetGltfModelInfo(const MikuPan_GltfModel *model,
                             MikuPan_GltfModelInfo *out_info)
{
    if (model == nullptr || out_info == nullptr)
    {
        return 0;
    }

    out_info->mesh_count = (int)model->meshes.size();
    out_info->material_count = (int)model->materials.size();
    out_info->texture_count = model->texture_count;
    out_info->vertex_count = model->vertex_count;
    out_info->index_count = model->index_count;
    out_info->skinned_mesh_count = model->skinned_mesh_count;
    out_info->bone_count = model->bone_count;
    out_info->node_mapped_mesh_count = model->node_mapped_mesh_count;
    out_info->node_unmapped_mesh_count = model->node_unmapped_mesh_count;
    return 1;
}
