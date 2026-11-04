#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_RMLUI_H
#define MIKUPAN_RMLUI_H

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"


MIKUPAN_NAMESPACE_BEGIN

#ifdef __cplusplus
MIKUPAN_EXTERN_C {
#endif

void MikuPan_RmlUiInit(SDL_Window* window);
void MikuPan_RmlUiRequestItemIcons(void);
int MikuPan_RmlUiItemIconsReady(void);
void MikuPan_RmlUiStartFrame(void);
void MikuPan_RmlUiRender(void);
void MikuPan_RmlUiProcessEvent(SDL_Event* event);
void MikuPan_RmlUiToggleHiWindow(void);
void MikuPan_RmlUiShutdown(void);

#ifdef __cplusplus
}
#endif


MIKUPAN_NAMESPACE_END
#endif // MIKUPAN_RMLUI_H
