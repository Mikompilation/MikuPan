#ifndef MIKUPAN_UI_SAVES_H
#define MIKUPAN_UI_SAVES_H
struct SDL_Window;
struct SDL_GPUDevice;
namespace MikuPanUiSaves
{
void Init(SDL_GPUDevice *device);
void Shutdown();
void MenuItem();
bool IsOpen();
void Draw(SDL_Window *window);
} // namespace MikuPanUiSaves
#endif
