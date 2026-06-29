#include "mikupan/mikupan_audio_bus.h"

#include "enums.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_utils.h"

static u_int g_mikupan_audio_revision = 1;

u_int MikuPan_AudioGetRevision(void)
{
    return g_mikupan_audio_revision;
}

void MikuPan_AudioBumpRevision(void)
{
    g_mikupan_audio_revision++;
    if (g_mikupan_audio_revision == 0)
    {
        g_mikupan_audio_revision = 1;
    }
}

int MikuPan_AudioBusForAdpcmMode(int adpcm_mode)
{
    switch (adpcm_mode)
    {
        case ADPCM_MODE_HISO:
        case ADPCM_MODE_AUTOG:
        case ADPCM_MODE_GHOST:
        case ADPCM_MODE_BTLMODE:
            return MIKUPAN_AUDIO_BUS_BATTLE_BGM;

        case ADPCM_MODE_MAP:
        case ADPCM_MODE_FURN:
        case ADPCM_MODE_MAGATOKI:
        case ADPCM_MODE_GOVER:
            return MIKUPAN_AUDIO_BUS_AMBIENT_BGM;

        /* These ADPCM modes are streamed, but they are not really "BGM" in
         * the user-facing sense. EVENT contains the AVMC character/dialogue
         * streams, SCENE contains in-engine scene audio, and SHINKAN/TAPE/SOUL/
         * PUZZLE/GDEAD are one-off voice/stinger/event streams. Keep them on
         * Ambient SE so lowering room/battle music does not mute story audio
         * such as Mafuyu's intro monologue.
         */
        case ADPCM_MODE_EVENT:
        case ADPCM_MODE_GDEAD:
        case ADPCM_MODE_TAPE:
        case ADPCM_MODE_SOUL:
        case ADPCM_MODE_PUZZLE:
        case ADPCM_MODE_SHINKAN:
        case ADPCM_MODE_SCENE:
        case ADPCM_MODE_MOVIE:
            return MIKUPAN_AUDIO_BUS_AMBIENT_SE;

        case ADPCM_MODE_NONE:
        default:
            return MIKUPAN_AUDIO_BUS_AMBIENT_BGM;
    }
}

int MikuPan_AudioBusForSe(int se_no, int request_mode)
{
    /* Mode 3 is the enemy-following SE path. It catches the important
     * haunting/menace/attack loops even when a specific SE id is reused. */
    if (request_mode == 3)
    {
        return MIKUPAN_AUDIO_BUS_BATTLE_SE;
    }

    /* Enemy hit/purify/warp group and three ghost voice banks from iopse.h.
     * Kept numeric here so this helper stays independent of the IOP-side
     * header's large enum soup. */
    if ((se_no >= 29 && se_no <= 32)
        || (se_no >= 54 && se_no <= 86)
        || (se_no >= 94 && se_no <= 97))
    {
        return MIKUPAN_AUDIO_BUS_BATTLE_SE;
    }

    return MIKUPAN_AUDIO_BUS_AMBIENT_SE;
}

static float MikuPan_AudioGetMasterScale(void)
{
    return MikuPan_ClampFloat(mikupan_configuration.audio.master, 0.0f, 1.0f);
}

static float MikuPan_AudioGetBusOnlyScale(int audio_bus)
{
    const MikuPan_ConfigAudio* audio = &mikupan_configuration.audio;

    switch (audio_bus)
    {
        case MIKUPAN_AUDIO_BUS_BATTLE_BGM:
            return MikuPan_ClampFloat(audio->battle_bgm, 0.0f, 1.0f);
        case MIKUPAN_AUDIO_BUS_AMBIENT_SE:
            return MikuPan_ClampFloat(audio->ambient_se, 0.0f, 1.0f);
        case MIKUPAN_AUDIO_BUS_BATTLE_SE:
            return MikuPan_ClampFloat(audio->battle_se, 0.0f, 1.0f);
        case MIKUPAN_AUDIO_BUS_AMBIENT_BGM:
        default:
            return MikuPan_ClampFloat(audio->ambient_bgm, 0.0f, 1.0f);
    }
}

static float MikuPan_AudioGetSeScale(int audio_bus)
{
    return MikuPan_AudioGetMasterScale() * MikuPan_AudioGetBusOnlyScale(audio_bus);
}

static u_short MikuPan_AudioScaleVolumeByScale(u_short volume,
                                                float scale,
                                                u_short max_volume)
{
    float scaled;

    if (volume > max_volume)
    {
        volume = max_volume;
    }

    scaled = (float) volume * MikuPan_ClampFloat(scale, 0.0f, 1.0f);
    if (scaled <= 0.0f)
    {
        return 0;
    }
    if (scaled >= (float) max_volume)
    {
        return max_volume;
    }

    return (u_short) (scaled + 0.5f);
}

u_short MikuPan_AudioScaleAdpcmMasterVolume(u_short volume,
                                             int adpcm_mode,
                                             u_short max_volume)
{
    (void) adpcm_mode;

    /* ADPCM master volume is global. Do not apply Ambient/Battle BGM here or
     * one muted BGM bus can leave every non-PSS ADPCM stream silent. Bus
     * volumes are applied to individual ADPCM play/position commands instead. */
    return MikuPan_AudioScaleVolumeByScale(
        volume, MikuPan_AudioGetMasterScale(), max_volume);
}

u_short MikuPan_AudioScaleAdpcmStreamVolume(u_short volume,
                                             int adpcm_mode,
                                             u_short max_volume)
{
    return MikuPan_AudioScaleVolumeByScale(
        volume,
        MikuPan_AudioGetBusOnlyScale(MikuPan_AudioBusForAdpcmMode(adpcm_mode)),
        max_volume);
}

u_short MikuPan_AudioScaleSeVolume(u_short volume,
                                    int audio_bus,
                                    u_short max_volume)
{
    return MikuPan_AudioScaleVolumeByScale(
        volume, MikuPan_AudioGetSeScale(audio_bus), max_volume);
}
