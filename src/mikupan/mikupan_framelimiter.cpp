#include "mikupan_framelimiter.h"

static Uint64 MikuPan_SecondsToCounterTicks(const MikuPan_FrameLimiter* limiter,
                                            double seconds)
{
    if (!limiter || limiter->frequency == 0)
    {
        return 0;
    }

    if (seconds <= 0.0)
    {
        return 0;
    }

    return (Uint64) (seconds * (double) limiter->frequency + 0.5);
}

static double MikuPan_CounterTicksToSeconds(const MikuPan_FrameLimiter* limiter,
                                            Uint64 ticks)
{
    if (!limiter || limiter->frequency == 0)
    {
        return 0.0;
    }

    return (double) ticks / (double) limiter->frequency;
}

void MikuPan_FrameLimiter_Init(MikuPan_FrameLimiter* limiter, double target_fps)
{
    if (!limiter)
    {
        return;
    }

    limiter->frequency = SDL_GetPerformanceFrequency();

    limiter->spin_wait_seconds = 0.0005;
    limiter->initialized = true;

    MikuPan_FrameLimiter_SetFPS(limiter, target_fps);
    MikuPan_FrameLimiter_Reset(limiter);
}

void MikuPan_FrameLimiter_SetFPS(MikuPan_FrameLimiter* limiter,
                                 double target_fps)
{
    if (!limiter)
    {
        return;
    }

    if (target_fps <= 0.0)
    {
        target_fps = 60.0;
    }

    limiter->target_fps = target_fps;
    limiter->target_seconds = 1.0 / target_fps;
}

void MikuPan_FrameLimiter_Reset(MikuPan_FrameLimiter* limiter)
{
    if (!limiter)
    {
        return;
    }

    limiter->next_counter = SDL_GetPerformanceCounter();
}

void MikuPan_FrameLimiter_Wait(MikuPan_FrameLimiter* limiter)
{
    Uint64 now;
    Uint64 target_counter;
    Uint64 frame_ticks;
    Uint64 spin_ticks;

    if (!limiter || !limiter->initialized || limiter->frequency == 0)
    {
        return;
    }

    frame_ticks =
        MikuPan_SecondsToCounterTicks(limiter, limiter->target_seconds);
    spin_ticks =
        MikuPan_SecondsToCounterTicks(limiter, limiter->spin_wait_seconds);

    target_counter = limiter->next_counter + frame_ticks;

    for (;;)
    {
        Uint64 remaining_ticks;
        double remaining_seconds;
        Uint64 sleep_ns;

        now = SDL_GetPerformanceCounter();

        if (now >= target_counter)
        {
            break;
        }

        remaining_ticks = target_counter - now;

        /*
            Sleep only if there is enough time left.

            We intentionally leave spin_wait_seconds unslept because
            OS sleep calls can wake up slightly late.
        */
        if (remaining_ticks > spin_ticks)
        {
            remaining_seconds = MikuPan_CounterTicksToSeconds(
                limiter, remaining_ticks - spin_ticks);

            sleep_ns = (Uint64) (remaining_seconds * 1000000000.0);

            if (sleep_ns > 0)
            {
                SDL_DelayNS(sleep_ns);
            }
        }
        else
        {
            /*
                Final accurate wait.

                SDL_DelayNS is not precise enough for the last sub-ms part,
                so we spin until the exact target counter.
            */
            while (SDL_GetPerformanceCounter() < target_counter)
            {
                /*
                    Busy wait.
                    Do not call SDL_DelayNS here if you want tighter pacing.
                */
            }

            break;
        }
    }

    /*
        Advance based on the previous scheduled frame time, not based on "now".
        This prevents long-term drift.
    */
    limiter->next_counter = target_counter;

    /*
        If the frame took too long, do not try to "catch up" by running many
        frames instantly. Resync to current time instead.
    */
    now = SDL_GetPerformanceCounter();

    if (now > limiter->next_counter + frame_ticks)
    {
        limiter->next_counter = now;
    }
}