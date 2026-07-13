#ifndef MIKUPAN_FRAME_LIMITER_H
#define MIKUPAN_FRAME_LIMITER_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MikuPan_FrameLimiter
{
    double target_fps;
    double target_seconds;

    Uint64 frequency;
    Uint64 next_counter;

    /*
        How much time to leave for spin-waiting.

        Higher value:
            - More accurate frame pacing
            - More CPU usage

        Lower value:
            - Less CPU usage
            - Slightly more jitter from OS sleep overshoot

        0.0005 = 0.5 ms, good default.
    */
    double spin_wait_seconds;

    /*
        Set to SDL_TRUE after initialization.
    */
    bool initialized;
} MikuPan_FrameLimiter;

void MikuPan_FrameLimiter_Init(MikuPan_FrameLimiter* limiter,
                               double target_fps);
void MikuPan_FrameLimiter_SetFPS(MikuPan_FrameLimiter* limiter,
                                 double target_fps);
void MikuPan_FrameLimiter_Reset(MikuPan_FrameLimiter* limiter);
void MikuPan_FrameLimiter_Wait(MikuPan_FrameLimiter* limiter);

#ifdef __cplusplus
}
#endif

#endif