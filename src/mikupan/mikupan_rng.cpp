#include "mikupan/mikupan_region.h"
#include "mikupan_rng.h"

#include "SDL3/SDL_stdinc.h"

#include <stdlib.h>


MIKUPAN_NAMESPACE_BEGIN

int MikuPan_Rand(void)
{
    return SDL_rand(MikuPan_RAND_MAX);
}

MIKUPAN_NAMESPACE_END
