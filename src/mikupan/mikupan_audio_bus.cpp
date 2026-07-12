#include "mikupan/mikupan_audio_bus.h"

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

float MikuPan_AudioGetMasterScale(void)
{
    return MikuPan_ClampFloat(mikupan_configuration.audio.master, 0.0f, 1.0f);
}
