#include "mikupan/mikupan_region.h"
struct SDL_Window;
struct SDL_GPUDevice;
#ifndef MIKUPAN_UI_SAVES_H
#define MIKUPAN_UI_SAVES_H



MIKUPAN_NAMESPACE_BEGIN

namespace MikuPanUiSaves
{
void Init(SDL_GPUDevice *device);
void Shutdown();
void MenuItem();
bool IsOpen();
void Draw(SDL_Window *window);
} // namespace MikuPanUiSaves

MIKUPAN_NAMESPACE_END
#endif
