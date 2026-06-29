#include "mikupan_rng.h"

#include "SDL3/SDL_stdinc.h"

#include <stdlib.h>

int MikuPan_Rand(void)
{
    return SDL_rand(MikuPan_RAND_MAX);
}
