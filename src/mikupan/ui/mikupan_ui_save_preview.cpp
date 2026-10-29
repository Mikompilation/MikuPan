#include "mikupan_ui_save_preview.h"
#include "mikupan_rml_save_load.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>
#include <map>
namespace MikuPanUiSavePreview
{
namespace
{
SDL_GPUDevice *device;
struct Texture
{
    SDL_GPUTexture *gpu = nullptr;
    int width = 0, height = 0;
};
std::map<int, Texture> textures;
Texture Load(const MikuPanSave::SlotInfo &slot)
{
    Texture texture;
    MikuPanSave::Bytes rgba;
    if (!MikuPan_RmlSaveLoadBuildTransferPreview(slot.chapter, slot.area, slot.finder, rgba))
        return texture;
    texture.width = 320;
    texture.height = 180;
    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    info.width = unsigned(texture.width);
    info.height = unsigned(texture.height);
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    texture.gpu = SDL_CreateGPUTexture(device, &info);
    SDL_GPUTransferBufferCreateInfo upload_info{};
    upload_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    upload_info.size = unsigned(rgba.size());
    auto *upload = SDL_CreateGPUTransferBuffer(device, &upload_info);
    void *mapped = upload ? SDL_MapGPUTransferBuffer(device, upload, false) : nullptr;
    if (!texture.gpu || !mapped)
    {
        if (texture.gpu)
            SDL_ReleaseGPUTexture(device, texture.gpu);
        if (upload)
            SDL_ReleaseGPUTransferBuffer(device, upload);
        texture.gpu = nullptr;
        return texture;
    }
    std::memcpy(mapped, rgba.data(), rgba.size());
    SDL_UnmapGPUTransferBuffer(device, upload);
    auto *command = SDL_AcquireGPUCommandBuffer(device);
    auto *copy = command ? SDL_BeginGPUCopyPass(command) : nullptr;
    if (copy)
    {
        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = upload;
        SDL_GPUTextureRegion target{};
        target.texture = texture.gpu;
        target.w = info.width;
        target.h = info.height;
        target.d = 1;
        SDL_UploadToGPUTexture(copy, &source, &target, false);
        SDL_EndGPUCopyPass(copy);
        if (!SDL_SubmitGPUCommandBuffer(command))
        {
            SDL_ReleaseGPUTexture(device, texture.gpu);
            texture.gpu = nullptr;
        }
    }
    else
    {
        if (command)
            SDL_CancelGPUCommandBuffer(command);
        SDL_ReleaseGPUTexture(device, texture.gpu);
        texture.gpu = nullptr;
    }
    SDL_ReleaseGPUTransferBuffer(device, upload);
    return texture;
}
} // namespace
void Init(SDL_GPUDevice *gpu) { device = gpu; }
void Shutdown()
{
    if (device)
        for (auto &[id, t] : textures)
            if (t.gpu)
                SDL_ReleaseGPUTexture(device, t.gpu);
    textures.clear();
    device = nullptr;
}
void Draw(const MikuPanSave::SlotInfo &slot, ImVec2 size)
{
    const int file = int(slot.chapter) * 512 + slot.area * 2 + int(slot.finder);
    if (device && slot.occupied && !textures.count(file))
    {
        auto texture = Load(slot);
        if (texture.gpu)
            textures.emplace(file, texture);
    }
    const auto found = textures.find(file);
    if (found != textures.end() && found->second.gpu)
        ImGui::Image((ImTextureID)(intptr_t)found->second.gpu, size);
    else
    {
        const auto at = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + size.x, at.y + size.y),
                                                  IM_COL32(32, 35, 38, 255));
        ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + 5, at.y + size.y / 2 - 6),
                                            IM_COL32(140, 145, 150, 255),
                                            slot.occupied ? "No preview" : "Empty");
        ImGui::Dummy(size);
    }
}
} // namespace MikuPanUiSavePreview
