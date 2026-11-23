#include "engine_api.h"
#include "mikupan/mikupan_region.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "enums.h"
#include "mikupan/io/mikupan_file.h"

MIKUPAN_NAMESPACE_BEGIN
SDL_AppResult SDL_AppInit(void**, int, char**);
SDL_AppResult SDL_AppEvent(void*, SDL_Event*);
SDL_AppResult SDL_AppIterate(void*);
void SDL_AppQuit(void*, SDL_AppResult);
MIKUPAN_NAMESPACE_END

namespace MikuPanHost
{
const Engine& MIKUPAN_ENGINE_FACTORY()
{
    static const Engine engine = {
#ifdef BUILD_EU_VERSION
        Region::EU,
#else
        Region::US,
#endif
        MIKUPAN_NAMESPACE::GetFileNameCount,
        MIKUPAN_NAMESPACE::IG_MSG_OBJ,
        MIKUPAN_NAMESPACE::SDL_AppInit,
        MIKUPAN_NAMESPACE::SDL_AppEvent,
        MIKUPAN_NAMESPACE::SDL_AppIterate,
        MIKUPAN_NAMESPACE::SDL_AppQuit,
        MIKUPAN_NAMESPACE::MikuPan_GetBaseDirectory
    };
    return engine;
}
}
