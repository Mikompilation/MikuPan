#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_FONT_TEXTURES_H
#define MIKUPAN_FONT_TEXTURES_H

MIKUPAN_NAMESPACE_BEGIN

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

MIKUPAN_NAMESPACE_END
#endif
