#include "libpc.h"

#include <SDL3/SDL.h>

uint64_t start_timer = 0;

#include <SDL3/SDL.h>
#include <stdint.h>

#define MIKUPAN_PS2_FRAME_COUNTER_TICKS 4860800.0

static uint64_t g_perf_base = 0;

void scePcStart(int control, int counter0, int counter1)
{
    g_perf_base = SDL_GetPerformanceCounter();
}

void scePcStop()
{
}

int scePcGetCounter0(void)
{
    uint64_t now = SDL_GetPerformanceCounter();
    uint64_t delta = now - g_perf_base;

    double seconds = (double)delta / (double)SDL_GetPerformanceFrequency();

    /*
        Scale real elapsed time into the game's expected PS2 counter domain.

        If the game is running at 60 Hz, then:
            1 frame ~= 1.0 / 60.0 seconds
            1 frame == 4860800 game counter ticks
    */
    double ps2_ticks = seconds * 60.0 * MIKUPAN_PS2_FRAME_COUNTER_TICKS;

    if (ps2_ticks < 0.0)
        return 0;

    if (ps2_ticks > 4294967295.0)
        return 0xffffffffu;

    return (uint32_t)ps2_ticks;
}