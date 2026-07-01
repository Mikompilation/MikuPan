#ifndef MIKUPAN_MIKUPAN_AUDIO_BUS_H
#define MIKUPAN_MIKUPAN_AUDIO_BUS_H

#include "typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum MikuPan_AudioBus
{
    MIKUPAN_AUDIO_BUS_AMBIENT_BGM = 0,
    MIKUPAN_AUDIO_BUS_BATTLE_BGM = 1,
    MIKUPAN_AUDIO_BUS_AMBIENT_SE = 2,
    MIKUPAN_AUDIO_BUS_BATTLE_SE = 3,
};

u_int MikuPan_AudioGetRevision(void);
void MikuPan_AudioBumpRevision(void);
int MikuPan_AudioBusForAdpcmMode(int adpcm_mode);
int MikuPan_AudioBusForSe(int se_no, int request_mode);
u_short MikuPan_AudioScaleAdpcmMasterVolume(u_short volume,
                                             int adpcm_mode,
                                             u_short max_volume);
u_short MikuPan_AudioScaleAdpcmStreamVolume(u_short volume,
                                             int adpcm_mode,
                                             u_short max_volume);
u_short MikuPan_AudioScaleSeVolume(u_short volume,
                                    int audio_bus,
                                    u_short max_volume);

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_MIKUPAN_AUDIO_BUS_H
