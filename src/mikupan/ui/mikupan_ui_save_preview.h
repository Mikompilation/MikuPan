#ifndef MIKUPAN_UI_SAVE_PREVIEW_H
#define MIKUPAN_UI_SAVE_PREVIEW_H
#include "mikupan/save/mikupan_save_transfer.h"
#include <imgui.h>
struct SDL_GPUDevice;
namespace MikuPanUiSavePreview
{
void Init(SDL_GPUDevice *device);
void Shutdown();
void Draw(const MikuPanSave::SlotInfo &slot, ImVec2 size);
} // namespace MikuPanUiSavePreview
#endif
