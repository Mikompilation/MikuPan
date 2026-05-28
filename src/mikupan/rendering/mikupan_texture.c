#include "mikupan_renderer_internal.h"
#include "graphics/graph2d/message.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"
#include <stdlib.h>

static GLfloat g_max_aniso = -1.0f;
static MikuPan_TextureInfo *fnt_texture[6] = {0};
static MikuPan_TextureInfo *curr_fnt_texture = NULL;

MikuPan_TextureInfo *MikuPan_GetCurrentFontTexture(void)
{
    return curr_fnt_texture;
}

void MikuPan_TextureShutdown(void)
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] != NULL)
        {
            glad_glDeleteTextures(1, (const GLuint *)&fnt_texture[i]->id);
            free(fnt_texture[i]);
            fnt_texture[i] = NULL;
        }
    }

    curr_fnt_texture = NULL;
}

static GLfloat MikuPan_GetMaxAniso(void)
{
    if (g_max_aniso < 0.0f)
    {
        glad_glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g_max_aniso);
    }

    return g_max_aniso;
}

MikuPan_TextureInfo *MikuPan_CreateGLTexture(sceGsTex0 *tex0)
{
    GLuint tex = 0;

    int width = 1 << tex0->TW;
    int height = 1 << tex0->TH;

    uint64_t hash = 0;
    void *pixels = MikuPan_GsDownloadTexture(tex0, &hash);

    if (hash == 0)
    {
        return NULL;
    }

    glad_glGenTextures(1, &tex);
    MikuPan_BindTexture2DCached(tex);
    glad_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                      width, height, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, pixels);
    glad_glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, MikuPan_GetMaxAniso());
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glad_glGenerateMipmap(GL_TEXTURE_2D);
    MikuPan_BindTexture2DCached(0);

    MikuPan_TextureInfo *texture_info = malloc(sizeof(MikuPan_TextureInfo));
    texture_info->height = height;
    texture_info->width = width;
    texture_info->id = tex;
    texture_info->tex0 = *(uint64_t*)tex0;
    texture_info->hash = hash;

    MikuPan_AddTexture(hash, texture_info);

    free(pixels);

    return texture_info;
}

void MikuPan_SetTexture(sceGsTex0 *tex0)
{
    // L1: lookup by raw tex0 register value. Avoids the per-call XXH3 over
    // GS memory (which can run into many MB/frame for textured scenes).
    uint64_t tex0_value = *(uint64_t *)tex0;

    // Sub-section breakdown of PERF_SECT_SC_TEXTURE — each step times itself
    // separately so the perf graph can show whether the cost is the L1 hash
    // probe, the XXH3 over GS memory, the L2 lookup, an allocation/upload,
    // or just glBindTexture.
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_TextureInfo *texture_info = MikuPan_LookupTextureByTex0(tex0_value);
    MikuPan_PerfEnd(PERF_SECT_TEX_L1_LOOKUP, _t);

    if (texture_info == NULL)
    {
        MikuPan_PerfTexL1Miss();
        // L2: hash-based lookup. Same texture data may be referenced by
        // different tex0 values; the GS-memory hash unifies them.
        _t = MikuPan_PerfBegin();
        uint64_t hash = MikuPan_GetTextureHash(tex0);
        MikuPan_PerfEnd(PERF_SECT_TEX_HASH, _t);

        _t = MikuPan_PerfBegin();
        texture_info = MikuPan_GetTextureInfo(hash);
        MikuPan_PerfEnd(PERF_SECT_TEX_L2_LOOKUP, _t);

        if (texture_info == NULL)
        {
            _t = MikuPan_PerfBegin();
            texture_info = MikuPan_CreateGLTexture(tex0);
            MikuPan_PerfEnd(PERF_SECT_TEX_CREATE, _t);
        }

        if (texture_info != NULL)
        {
            MikuPan_RegisterTextureForTex0(tex0_value, tex0, texture_info);
        }
    }
    else
    {
        MikuPan_PerfTexL1Hit();
    }

    if (texture_info != NULL)
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindTexture2DCached(texture_info->id);
        MikuPan_PerfEnd(PERF_SECT_TEX_BIND, _t);
    }
}

void MikuPan_SetupFntTexture()
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == NULL)
        {
            fnt_texture[i] = MikuPan_CreateGLTexture((sceGsTex0 *) &fntdat[i].tex0);
        }
    }

    curr_fnt_texture = fnt_texture[0];
}

void MikuPan_SetFontTexture(int fnt)
{
    curr_fnt_texture = fnt_texture[fnt];
}

void MikuPan_DeleteTexture(MikuPan_TextureInfo *texture_info)
{
    if (texture_info == NULL)
    {
        return;
    }

    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] != NULL && fnt_texture[i]->id == texture_info->id)
        {
            return;
        }
    }

    glad_glDeleteTextures(1, (const GLuint *) &texture_info->id);
    free(texture_info);
}
