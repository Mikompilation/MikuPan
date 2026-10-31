#ifndef MIKUPAN_FONT_TEXTURES_H
#define MIKUPAN_FONT_TEXTURES_H
namespace MikuPanFonts
{
// One startup pass; neither font reloads nor renderer calls compute fingerprints.
void Prepare();
void ResetLoads();
void LoadRequested(int id, int file, void *destination, int sector, int size);
void LoadCompleted(int id);
int LoadedLanguage();
unsigned int Texture(int bank, int width, int height);
void ShutdownGpu();
}
#endif
